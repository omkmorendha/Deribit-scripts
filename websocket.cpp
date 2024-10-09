#include <iostream>
#include <string>
#include <thread>
#include <unordered_map>
#include <set>
#include <queue>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>
#include <curl/curl.h>
#include <jsoncpp/json/json.h>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
using tcp = asio::ip::tcp;

const int DEPTH = 3;
const int PORT = 4221;
const int WAIT_TIME = 5;
const std::string API_URL = "https://test.deribit.com/api/v2/public/get_order_book?instrument_name=";

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

std::string get_order_book(const std::string& symbol) {
    CURL* curl;
    CURLcode res;
    std::string readBuffer;
    std::string url = API_URL + symbol + "&depth=" + std::to_string(DEPTH);

    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
    }

    return readBuffer;
}

class session : public std::enable_shared_from_this<session> {
    websocket::stream<tcp::socket> ws_;
    asio::steady_timer timer_;
    std::set<std::string> subscribed_symbols_;
    std::queue<std::string> message_queue_;
    bool writing_in_progress_ = false;

public:
    session(tcp::socket socket) : ws_(std::move(socket)), timer_(ws_.get_executor()) {}

    void start() {
        ws_.async_accept([self = shared_from_this()](beast::error_code ec) {
            if (!ec) self->read_message();
        });
    }

    void read_message() {
        auto buffer = std::make_shared<beast::flat_buffer>();
        ws_.async_read(*buffer, [self = shared_from_this(), buffer](beast::error_code ec, std::size_t bytes_transferred) {
            if (!ec) {
                std::string message = beast::buffers_to_string(buffer->data());
                self->handle_subscription(message);
                self->read_message();
            }
        });
    }

    void handle_subscription(const std::string& message) {
        subscribed_symbols_.insert(message);
        std::cout << "Client subscribed to: " << message << std::endl;
        send_order_book_updates();
    }

    void send_order_book_updates() {
        if (writing_in_progress_) {
            return;  // Don't start a new write if one is in progress
        }

        if (!subscribed_symbols_.empty()) {
            for (const auto& symbol : subscribed_symbols_) {
                std::string order_book = get_order_book(symbol);
                message_queue_.push(order_book);
            }
        }

        timer_.expires_after(std::chrono::seconds(WAIT_TIME));
        timer_.async_wait([self = shared_from_this()](beast::error_code ec) {
            if (!ec) {
                self->write_next_message();  // Write the next message in the queue
            }
        });
    }

    void write_next_message() {
        if (writing_in_progress_ || message_queue_.empty()) {
            return;
        }

        std::string message = message_queue_.front();
        message_queue_.pop();

        ws_.text(true);
        writing_in_progress_ = true;
        ws_.async_write(asio::buffer(message), [self = shared_from_this()](beast::error_code ec, std::size_t) {
            self->writing_in_progress_ = false;

            if (!ec) {
                if (!self->message_queue_.empty()) {
                    self->write_next_message();  // Continue writing the next message
                } else {
                    self->send_order_book_updates();  // Queue up more updates
                }
            } else {
                std::cerr << "Failed to send message: " << ec.message() << std::endl;
            }
        });
    }
};

class server {
    tcp::acceptor acceptor_;
    tcp::socket socket_;

public:
    server(asio::io_context& ioc, tcp::endpoint endpoint) : acceptor_(ioc), socket_(ioc) {
        acceptor_.open(endpoint.protocol());
        acceptor_.set_option(asio::socket_base::reuse_address(true));
        acceptor_.bind(endpoint);
        acceptor_.listen();
        accept_connection();
    }

    void accept_connection() {
        acceptor_.async_accept(socket_, [this](beast::error_code ec) {
            if (!ec) {
                std::make_shared<session>(std::move(socket_))->start();
            }
            accept_connection();
        });
    }
};

int main() {
    try {
        asio::io_context ioc;

        server srv(ioc, tcp::endpoint(tcp::v4(), PORT));

        std::cout << "WebSocket server started on port " << PORT << std::endl;

        ioc.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    return 0;
}
