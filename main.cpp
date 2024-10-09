#include <iostream>
#include <cstdlib>
#include <curl/curl.h>
#include <json/json.h>
#include <string>
#include <chrono>
#include <thread>

using namespace std;
using namespace std::chrono;

string access_token;
steady_clock::time_point token_creation_time;
int token_expiration_time = 900;

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

bool is_token_expired() {
    steady_clock::time_point current_time = steady_clock::now();
    duration<double> elapsed_seconds = current_time - token_creation_time;
    return elapsed_seconds.count() >= token_expiration_time;
}

void refresh_token() {
    // Fetch environment variables and check for nullptr
    const char* client_id_env = getenv("CLIENT_ID");
    const char* api_key_env = getenv("API_KEY");

    if (client_id_env == nullptr || api_key_env == nullptr) {
        cerr << "Environment variables CLIENT_ID or API_KEY not set." << endl;
        return;
    }

    string client_id = client_id_env;
    string api_key = api_key_env;
    string token_url = "https://test.deribit.com/api/v2/public/auth?client_id=" + client_id + "&client_secret=" + api_key + "&grant_type=client_credentials";
    
    CURL* curl;
    CURLcode res;
    string readBuffer;

    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, token_url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

        // Perform GET request
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
    }

    // Parse the JSON response
    Json::CharReaderBuilder reader;
    Json::Value root;
    string errors;

    std::istringstream s(readBuffer);
    if (Json::parseFromStream(reader, s, &root, &errors)) {
        if (root["result"].isMember("access_token")) {
            access_token = root["result"]["access_token"].asString();
            token_creation_time = steady_clock::now();
            token_expiration_time = root["result"]["expires_in"].asInt();
            cout << "New token acquired, valid for " << token_expiration_time << " seconds." << endl;
        } else {
            cout << "Failed to acquire token: " << root << endl;
        }
    } else {
        cout << "Failed to parse the JSON response: " << errors << endl;
    }
}

string get_token() {
    if (access_token.empty() || is_token_expired()) {
        refresh_token();
    }
    return access_token;
}

void place_order() {
    string token = get_token();
    if(token.empty()) {
        cout << "Authentication failed" << endl;
        return;
    }

    string instrument_name, label, type, direction;
    int amount;

    cout << "Enter direction (buy/sell): ";
    cin >> direction;
    cout << "Enter instrument name (e.g., ETH-PERPETUAL): ";
    cin >> instrument_name;
    cout << "Enter amount: ";
    cin >> amount;
    cout << "Enter order type (e.g., market, limit): ";
    cin >> type;
    cout << "Enter label (optional identifier for the order): ";
    cin >> label;

    string order_url = "https://test.deribit.com/api/v2/private/" + direction +
                       "?amount=" + to_string(amount) +
                       "&instrument_name=" + instrument_name +
                       "&label=" + label +
                       "&type=" + type;

    CURL* curl;
    CURLcode res;
    string readBuffer;

    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, order_url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, ("Authorization: Bearer " + token).c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
    }

    cout << "Order response: " << readBuffer << endl;

    Json::CharReaderBuilder reader;
    Json::Value root;
    string errors;

    std::istringstream s(readBuffer);
    if (Json::parseFromStream(reader, s, &root, &errors)) {
        cout << "Parsed Order Response: " << root << endl;
    } else {
        cout << "Failed to parse the JSON response: " << errors << endl;
    }
}


void get_positions() {
    string token = get_token();

    if (token.empty()) {
        cout << "Authentication failed" << endl;
        return;
    }

    string currency, kind;
    cout << "Enter currency (e.g., BTC): ";
    cin >> currency;
    cout << "Enter kind (e.g., future): ";
    cin >> kind;

    string positions_url = "https://test.deribit.com/api/v2/private/get_positions?currency=" + currency + "&kind=" + kind;
    
    CURL* curl;
    CURLcode res;
    string readBuffer;

    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, positions_url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, ("Authorization: Bearer " + token).c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
    }

    Json::CharReaderBuilder reader;
    Json::Value root;
    string errors;

    std::istringstream s(readBuffer);
    if (Json::parseFromStream(reader, s, &root, &errors)) {
        if (root.isMember("result")) {
            cout << "Positions result: " << root["result"] << endl;
        } else {
            cout << "No result field found in the response." << endl;
        }
    } else {
        cout << "Failed to parse the JSON response: " << errors << endl;
    }
}


void get_order_book() {
    string instrument_name;
    int depth;
    
    cout << "Enter instrument name (e.g., BTC-PERPETUAL): ";
    cin >> instrument_name;
    cout << "Enter depth (e.g., 5): ";
    cin >> depth;

    string order_book_url = "https://test.deribit.com/api/v2/public/get_order_book?instrument_name=" + instrument_name + "&depth=" + to_string(depth);
    
    CURL* curl;
    CURLcode res;
    string readBuffer;

    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, order_book_url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
    }

    cout << "Order book response: " << readBuffer << endl;

    Json::CharReaderBuilder reader;
    Json::Value root;
    string errors;

    std::istringstream s(readBuffer);
    if (Json::parseFromStream(reader, s, &root, &errors)) {
        cout << "Parsed JSON Order Book: " << root << endl;
    } else {
        cout << "Failed to parse the JSON response: " << errors << endl;
    }
}

void modify_order() {
    string token = get_token();
    if (token.empty()) {
        cout << "Authentication failed" << endl;
        return;
    }

    string order_id, advanced;
    int amount;
    double price;

    cout << "Enter order ID: ";
    cin >> order_id;
    cout << "Enter amount: ";
    cin >> amount;
    cout << "Enter price: ";
    cin >> price;
    cout << "Enter advanced type (e.g., implv): ";
    cin >> advanced;

    string modify_url = "https://test.deribit.com/api/v2/private/edit?order_id=" + order_id + 
                        "&amount=" + to_string(amount) + 
                        "&price=" + to_string(price) + 
                        "&advanced=" + advanced;

    CURL* curl;
    CURLcode res;
    string readBuffer;

    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, modify_url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, ("Authorization: Bearer " + token).c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
    }

    cout << "Modify order response: " << readBuffer << endl;

    Json::CharReaderBuilder reader;
    Json::Value root;
    string errors;
    std::istringstream s(readBuffer);
    if (Json::parseFromStream(reader, s, &root, &errors)) {
        cout << "Parsed Modify Order Response: " << root << endl;
    } else {
        cout << "Failed to parse the JSON response: " << errors << endl;
    }
}

void cancel_order() {
    string token = get_token();
    if (token.empty()) {
        cout << "Authentication failed" << endl;
        return;
    }

    string order_id;

    cout << "Enter order ID to cancel: ";
    cin >> order_id;

    string cancel_url = "https://test.deribit.com/api/v2/private/cancel?order_id=" + order_id;

    CURL* curl;
    CURLcode res;
    string readBuffer;

    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, cancel_url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, ("Authorization: Bearer " + token).c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
    }

    cout << "Cancel order response: " << readBuffer << endl;

    Json::CharReaderBuilder reader;
    Json::Value root;
    string errors;
    std::istringstream s(readBuffer);
    if (Json::parseFromStream(reader, s, &root, &errors)) {
        cout << "Parsed Cancel Order Response: " << root << endl;
    } else {
        cout << "Failed to parse the JSON response: " << errors << endl;
    }
}

void display_menu() {
    cout << "Select an action:" << endl;
    cout << "1. Authenticate" << endl;
    cout << "2. Place Order" << endl;
    cout << "3. Get Positions" << endl;
    cout << "4. Get Order Book" << endl;
    cout << "5. Modify Order" << endl;
    cout << "6. Cancel Order" << endl;
    cout << "7. Exit" << endl;
}

int main() {
    int choice;
    do {
        display_menu();
        cin >> choice;

        switch (choice) {
            case 1:
                cout << "Authenticating..." << endl;
                cout << "Token: " << get_token() << endl;
                break;
            case 2:
                cout << "Placing Order..." << endl;
                place_order();
                break;
            case 3:
                cout << "Fetching positions..." << endl;
                get_positions();
                break;
            case 4:
                cout << "Fetching order book..." << endl;
                get_order_book();
                break;
            case 5:
                cout << "Modifying order..." << endl;
                modify_order();
                break;
            case 6:
                cout << "Canceling order..." << endl;
                cancel_order();
                break;
            case 7:
                cout << "Exiting..." << endl;
                break;
            default:
                cout << "Invalid option, please try again." << endl;
        }
    } while (choice != 7);

    return 0;
}
