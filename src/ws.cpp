#include <iostream>
#include <set>
#include <string>
#include <unordered_map>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include "../lib/httplib.h"
#include "../lib/json11.hpp"
#include "../lib/semaphore11.h"

typedef websocketpp::server<websocketpp::config::asio> server;
extern server wss;
extern httplib::Server svr;
extern std::unordered_map<std::string, std::string> tokens;
extern std::string localAuth;
void httpThread();

using namespace std;
using websocketpp::lib::bind;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;

// pull out the type of messages sent by our config
typedef server::message_ptr message_ptr;

void handleHttpRequest(httplib::Request &request, httplib::Response &response) {
    size_t count = 0;
    httplib::detail::split(request.target.data(), request.target.data() + request.target.size(), '?',
                           [&](const char *b, const char *e) {
                               switch (count) {
                               case 0:
                                   request.path = httplib::detail::decode_url(std::string(b, e), false);
                                   break;
                               case 1: {
                                   if (e - b > 0) {
                                       httplib::detail::parse_query_text(std::string(b, e), request.params);
                                   }
                                   break;
                               }
                               default:
                                   break;
                               }
                               count++;
                           });

    try {
        bool routed = false;
        // Regular handler
        if (request.method == "GET" || request.method == "HEAD") {
            routed = svr.dispatch_request(request, response, svr.get_handlers_);
        } else if (request.method == "POST") {
            routed = svr.dispatch_request(request, response, svr.post_handlers_);
        } else if (request.method == "PUT") {
            routed = svr.dispatch_request(request, response, svr.put_handlers_);
        } else if (request.method == "DELETE") {
            routed = svr.dispatch_request(request, response, svr.delete_handlers_);
        } else if (request.method == "OPTIONS") {
            routed = svr.dispatch_request(request, response, svr.options_handlers_);
        } else if (request.method == "PATCH") {
            routed = svr.dispatch_request(request, response, svr.patch_handlers_);
        }
    } catch (std::exception &e) {
        response.status = 500;
        response.set_header("EXCEPTION_WHAT", e.what());
    } catch (...) {
        response.status = 500;
        response.set_header("EXCEPTION_WHAT", "UNKNOWN");
    }
}
void on_message(server *s, websocketpp::connection_hdl hdl, message_ptr msg) {
    string payload = msg->get_payload();
    // decode json
    string jsonerr = "";
    json11::Json json = json11::Json::parse(payload, jsonerr);
    // get message type
    string action = json["action"].string_value();
    // get message data
    json11::Json data = json["data"];
    // get message id
    string id = json["id"].string_value();
    if (action == "api") {
        httplib::Request request;
        httplib::Response response;
        request.method = data["method"].string_value();
        request.target = data["url"].string_value();
        request.body = data["body"].string_value();
        request.remote_addr = "WS";
        handleHttpRequest(request, response);
        string resp = response.body;
        json11::Json respjson = json11::Json::parse(resp, jsonerr);
        json11::Json res = json11::Json::object{
            {"id", id},
            {"action", "api"},
            {"data", json11::Json::object{{"status", response.status}, {"body", respjson}}}};
        string response_str = res.dump();
        s->send(hdl, response_str, websocketpp::frame::opcode::text);
        return;
    }
}

void on_http(server *s, websocketpp::connection_hdl hdl) {
    server::connection_ptr con = s->get_con_from_hdl(hdl);
    // forward to httplib
    httplib::Request request;
    httplib::Response response;
    response.status = 404;
    request.method = con->get_request().get_method();
    request.target = con->get_request().get_uri();
    websocketpp::http::parser::header_list headers = con->get_request().get_headers();
    for (auto &header : headers) {
        request.headers.insert(pair<string, string>(header.first, header.second));
    }
    request.body = con->get_request_body();
    handleHttpRequest(request, response);
    // send response
    con->set_body(response.body);
    con->set_status((websocketpp::http::status_code::value)response.status);
    // set response headers
    for (auto &header : response.headers) {
        con->replace_header(header.first, header.second);
    }
}

bool on_validate(server *s, websocketpp::connection_hdl hdl) {
    server::connection_ptr con = s->get_con_from_hdl(hdl);
    string url = con->get_request().get_uri();
    // conn url should be "/ws/{token}"
    // extract token
    size_t pos = url.find("/ws/");
    if (pos == string::npos) {
        return false;
    }
    string token = url.substr(pos + 4);
    if (localAuth != "" && token == localAuth) {
        return true;
    }
    string origin = con->get_request().get_header("Origin");
    if (tokens.find(token) == tokens.end()) {
        return false;
    }
    if (tokens[token][0] != '@' && origin != tokens[token]) {
        return false;
    }
    return true;
}

std::set<websocketpp::connection_hdl, std::owner_less<websocketpp::connection_hdl>> con_list;
void on_open(server *s, websocketpp::connection_hdl hdl) {
    con_list.insert(hdl);
}
void on_close(server *s, websocketpp::connection_hdl hdl) {
    con_list.erase(hdl);
}

void ws_broadcast(string msg) {
    for (auto &hdl : con_list) {
        wss.send(hdl, msg, websocketpp::frame::opcode::text);
    }
}
void ws_broadcast(string action, string msg) {
    json11::Json json = json11::Json::object{{"action", action}, {"data", msg}};
    string response_str = json.dump();
    ws_broadcast(response_str);
}

void ws_server() {
    httpThread();
    try {
        wss.set_access_channels(websocketpp::log::alevel::all);
        wss.clear_access_channels(websocketpp::log::alevel::frame_payload);
        wss.init_asio();
        wss.set_message_handler(bind(&on_message, &wss, ::_1, ::_2));
        wss.set_validate_handler(bind(&on_validate, &wss, ::_1));
        wss.set_http_handler(bind(&on_http, &wss, ::_1));
        wss.set_open_handler(bind(&on_open, &wss, ::_1));
        wss.set_close_handler(bind(&on_close, &wss, ::_1));
        wss.listen("127.0.0.1", "32333");
        wss.start_accept();
        wss.run();
    } catch (websocketpp::exception const &e) {
        std::cout << e.what() << std::endl;
    } catch (...) {
        std::cout << "other exception" << std::endl;
    }
}
void ws_stopall() {
    // stop listening
    wss.stop_listening();
    // disconnect all clients
    for (auto &hdl : con_list) {
        wss.close(hdl, websocketpp::close::status::going_away, "server shutdown");
    }
    // stop server
    wss.stop();
}