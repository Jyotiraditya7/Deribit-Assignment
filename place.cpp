#include <websocketpp/config/asio_client.hpp>  // Use TLS-enabled configuration
#include <websocketpp/client.hpp>
#include <nlohmann/json.hpp>  // For JSON handling
#include <iostream>
#include <string>
#include <websocketpp/common/asio_ssl.hpp>

typedef websocketpp::client<websocketpp::config::asio_tls_client> client;

using websocketpp::connection_hdl;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;
using json = nlohmann::json;

std::string auth_msg = R"({
  "jsonrpc" : "2.0",
  "id" : 9929,
  "method" : "public/auth",
  "params" : {
    "grant_type" : "client_credentials",
    "client_id" : "",/*replace key
    "client_secret" : "" // replace with key
  }
})";

std::string order_msg = R"({
  "jsonrpc": "2.0",
  "id": 5275,
  "method": "private/buy",
  "params": {
    "instrument_name": "ETH-PERPETUAL",
    "amount": 40,
    "type": "limit",
    "label": "market0000234",
    "price": 500
  }
})";

class WebSocketHandler {
public:
    WebSocketHandler() : m_open(false), m_done(false) {}

    void on_open(client* c, websocketpp::connection_hdl hdl) {
        m_open = true;
        m_hdl = hdl;
        c->send(hdl, auth_msg, websocketpp::frame::opcode::text);
    }

    void on_message(client* c, websocketpp::connection_hdl hdl, client::message_ptr msg) {
        // Parse the JSON message
        auto response = json::parse(msg->get_payload());

        // Check if authentication is successful
        if (response["id"].get<int>() == 9929) {
            std::cout << "Authentication successful: " << response.dump() << std::endl;

            // Send the order message
            c->send(hdl, order_msg, websocketpp::frame::opcode::text);
        }
        else if (response["id"].get<int>() == 5275) {
            std::cout << "Order response received: " << response.dump() << std::endl;
            m_done = true;  // Close after receiving the order response
        }
    }

    bool is_done() const {
        return m_done;
    }

    bool is_open() const {
        return m_open;
    }

private:
    bool m_open;
    bool m_done;
    connection_hdl m_hdl;
};

websocketpp::lib::shared_ptr<websocketpp::lib::asio::ssl::context> on_tls_init() {
    websocketpp::lib::shared_ptr<websocketpp::lib::asio::ssl::context> ctx =
        websocketpp::lib::make_shared<websocketpp::lib::asio::ssl::context>(
            websocketpp::lib::asio::ssl::context::tlsv12);

    try {
        ctx->set_options(websocketpp::lib::asio::ssl::context::default_workarounds |
                         websocketpp::lib::asio::ssl::context::no_sslv2 |
                         websocketpp::lib::asio::ssl::context::no_sslv3 |
                         websocketpp::lib::asio::ssl::context::single_dh_use);
    } catch (std::exception& e) {
        std::cout << "SSL context error: " << e.what() << std::endl;
    }
    return ctx;
}

int main() {
    client c;
    WebSocketHandler handler;

    try {
        c.set_access_channels(websocketpp::log::alevel::none);
        c.init_asio();

        c.set_tls_init_handler(bind(&on_tls_init));
        c.set_open_handler(bind(&WebSocketHandler::on_open, &handler, &c, ::_1));
        c.set_message_handler(bind(&WebSocketHandler::on_message, &handler, &c, ::_1, ::_2));

        websocketpp::lib::error_code ec;
        client::connection_ptr con = c.get_connection("wss://test.deribit.com/ws/api/v2", ec);
        if (ec) {
            std::cout << "Connection error: " << ec.message() << std::endl;
            return -1;
        }

        c.connect(con);

        c.run();

    } catch (websocketpp::exception const & e) {
        std::cout << "WebSocket Exception: " << e.what() << std::endl;
    } catch (...) {
        std::cout << "Other exception" << std::endl;
    }

    return 0;
}
