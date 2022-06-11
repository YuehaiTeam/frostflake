#include <iostream>
#include <set>
#include <sstream>
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
void notIdle();

using namespace std;
using websocketpp::lib::bind;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;

// pull out the type of messages sent by our config
typedef server::message_ptr message_ptr;

std::set<websocketpp::connection_hdl, std::owner_less<websocketpp::connection_hdl>> con_list;
// std::set<websocketpp::connection_hdl, std::owner_less<websocketpp::connection_hdl>> evs_list;
// std::set<websocketpp::connection_hdl, std::owner_less<websocketpp::connection_hdl>> evs_list_disconnected;

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
    notIdle();
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

// string gen_chunked_response(string str) {
//     string chunk = "";
//     // chunk size in hex
//     int size = str.size();
//     stringstream ss;
//     ss << std::hex << size;
//     chunk += ss.str();
//     chunk += "\r\n";
//     chunk += str;
//     chunk += "\r\n";
//     return chunk;
// }
// void send_evs_header(server *s, websocketpp::connection_hdl hdl) {
//     server::connection_ptr con = s->get_con_from_hdl(hdl);
//     con->defer_http_response();
//     string header = "HTTP/1.1 200 OK\r\n"
//                     "Content-Type: text/event-stream\r\n"
//                     "Cache-Control: no-cache\r\n"
//                     "Connection: keep-alive\r\n"
//                     "Access-Control-Allow-Origin: *\r\n"
//                     "\r\n";
//     header += "data: reconnect\n\n";
//     con->transport_con_type::async_write(header.c_str(), header.size(),
//                                          [hdl](const websocketpp::lib::error_code &ec) {
//                                              if (ec) {
//                                                  std::cout << "Error: " << ec.message() << std::endl;
//                                                  return;
//                                              }
//                                              evs_list.insert(hdl);
//                                          });
// }
// void send_evs_content(server *s, websocketpp::connection_hdl hdl, string msg) {
//     try {
//         server::connection_ptr con = s->get_con_from_hdl(hdl);
//     } catch (std::exception &e) {
//         std::cout << "GetEvsHandler Error:" << endl;
//         std::cout << e.what() << std::endl;
//         evs_list_disconnected.insert(hdl);
//         return;
//     }
//     server::connection_ptr con = s->get_con_from_hdl(hdl);
//     string chunk = "data: " + msg + "\n\n";
//     con->transport_con_type::async_write(chunk.c_str(), chunk.size(),
//                                          [hdl](const websocketpp::lib::error_code &ec) {
//                                              if (ec) {
//                                                  std::cout << "Error: " << ec.message() << std::endl;
//                                                  return;
//                                              }
//                                          });
// }

void on_http(server *s, websocketpp::connection_hdl hdl) {
    notIdle();
    server::connection_ptr con = s->get_con_from_hdl(hdl);
    websocketpp::lib::error_code ec = con->defer_http_response();
    // forward to httplib
    httplib::Request request;
    httplib::Response response;
    response.status = 404;
    request.method = con->get_request().get_method();
    request.target = con->get_request().get_uri();
    // // if url starts with /ws/
    // if (request.target.find("/ws/") == 0) {
    //     // token is in url after /ws/
    //     string token = request.target.substr(4);
    //     string origin = con->get_request().get_header("Origin");
    //     boolean tokenValid = false;
    //     if (localAuth != "" && token == localAuth) {
    //         tokenValid = true;
    //     } else if (tokens.find(token) != tokens.end() && (tokens[token][0] == '@' || origin == tokens[token])) {
    //         tokenValid = true;
    //     }
    //     if (tokenValid) {
    //         send_evs_header(s, hdl);
    //         return;
    //     }
    // }
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
    con->send_http_response();
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

void on_open(server *s, websocketpp::connection_hdl hdl) {
    con_list.insert(hdl);
    notIdle();
}
void on_close(server *s, websocketpp::connection_hdl hdl) {
    con_list.erase(hdl);
    notIdle();
}

void ws_broadcast(string msg) {
    for (auto &hdl : con_list) {
        wss.send(hdl, msg, websocketpp::frame::opcode::text);
    }
    // for (auto &hdl : evs_list) {
    //     send_evs_content(&wss, hdl, msg);
    // }
    // for (auto &hdl : evs_list_disconnected) {
    //     evs_list.erase(hdl);
    // }
    // evs_list_disconnected.empty();
}
void ws_broadcast(string action, string msg) {
    json11::Json json = json11::Json::object{{"action", action}, {"data", msg}};
    string response_str = json.dump();
    ws_broadcast(response_str);
}

int ws_count_connection() {
    return con_list.size();
}
int ws_count_evs_connection() {
    return 0;
//     return evs_list.size();
}

void ws_server() {
    httpThread();
    try {
        wss.init_asio();
        wss.set_message_handler(bind(&on_message, &wss, ::_1, ::_2));
        wss.set_validate_handler(bind(&on_validate, &wss, ::_1));
        wss.set_http_handler(bind(&on_http, &wss, ::_1));
        wss.set_open_handler(bind(&on_open, &wss, ::_1));
        wss.set_close_handler(bind(&on_close, &wss, ::_1));
        // wss.set_open_handshake_timeout(60 * 1e3);
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