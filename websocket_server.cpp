#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <nlohmann/json.hpp>
#include <iostream>
#include <map>
#include <string>
#include <set>
#include <thread>

typedef websocketpp::server<websocketpp::config::asio> server;
using websocketpp::connection_hdl;
using json = nlohmann::json;

class OrderBookServer {
public:
    OrderBookServer() {
        m_server.init_asio();
        m_server.set_open_handler(bind(&OrderBookServer::on_open, this, std::placeholders::_1));
        m_server.set_close_handler(bind(&OrderBookServer::on_close, this, std::placeholders::_1));
        m_server.set_message_handler(bind(&OrderBookServer::on_message, this, std::placeholders::_1, std::placeholders::_2));
    }

    void on_open(connection_hdl hdl) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_connections.insert(hdl);
    }

    void on_close(connection_hdl hdl) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_connections.erase(hdl);
        for (auto& [symbol, subscribers] : m_subscriptions) {
            subscribers.erase(hdl);
        }
    }

    void on_message(connection_hdl hdl, server::message_ptr msg) {
        json request = json::parse(msg->get_payload());
        std::string method = request["method"];

        if (method == "subscribe") {
            subscribe_to_symbol(hdl, request["params"]["symbol"]);
        } else if (method == "place") {
            handle_place_order(hdl, request);
        } else if (method == "cancel") {
            handle_cancel_order(hdl, request);
        } else if (method == "modify") {
            handle_modify_order(hdl, request);
        } else if (method == "orderbook") {
            send_orderbook(hdl, request);
        } else if (method == "currpos") {
            send_current_position(hdl, request);
        }
    }

    void run(uint16_t port) {
        m_server.listen(port);
        m_server.start_accept();
        std::thread([this] { m_server.run(); }).detach();
        broadcast_orderbook();
    }

private:
    server m_server;
    std::set<connection_hdl, std::owner_less<connection_hdl>> m_connections;
    std::map<std::string, std::set<connection_hdl, std::owner_less<connection_hdl>>> m_subscriptions;
    std::mutex m_mutex;

    void subscribe_to_symbol(connection_hdl hdl, const std::string& symbol) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_subscriptions[symbol].insert(hdl);
    }

    void broadcast_orderbook() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));

            for (const auto& [symbol, subscribers] : m_subscriptions) {
                json orderbook_update = get_orderbook(symbol);

                for (const auto& hdl : subscribers) {
                    m_server.send(hdl, orderbook_update.dump(), websocketpp::frame::opcode::text);
                }
            }
        }
    }

    json get_orderbook(const std::string& symbol) {
        json orderbook = {
            {"symbol", symbol},
            {"bids", {{"price", 5000}, {"amount", 1}}},
            {"asks", {{"price", 5100}, {"amount", 1}}}
        };
        return orderbook;
    }

    void handle_place_order(connection_hdl hdl, const json& request) {
        json response = {{"status", "order placed"}};
        m_server.send(hdl, response.dump(), websocketpp::frame::opcode::text);
    }

    void handle_cancel_order(connection_hdl hdl, const json& request) {
        json response = {{"status", "order cancelled"}};
        m_server.send(hdl, response.dump(), websocketpp::frame::opcode::text);
    }

    void handle_modify_order(connection_hdl hdl, const json& request) {
        json response = {{"status", "order modified"}};
        m_server.send(hdl, response.dump(), websocketpp::frame::opcode::text);
    }

    void send_orderbook(connection_hdl hdl, const json& request) {
        json response = get_orderbook(request["params"]["symbol"]);
        m_server.send(hdl, response.dump(), websocketpp::frame::opcode::text);
    }

    void send_current_position(connection_hdl hdl, const json& request) {
        json response = {
            {"currency", request["params"]["currency"]},
            {"positions", {{"type", "future"}, {"amount", 10}}}
        };
        m_server.send(hdl, response.dump(), websocketpp::frame::opcode::text);
    }
};

int main() {
    OrderBookServer server;
    server.run(9002);
    std::cout << "Server started on port 9002" << std::endl;
    while (true);
    return 0;
}
