#include "tts_server.h"
#include "tts_tradeapi.h"
#include <QObject>
#include <memory>
#include <QDebug>
#include <json.hpp>

using namespace restbed;
using json = nlohmann::json;

TTS_Server::TTS_Server(TTS_SettingObject setting)
{
    _setting = setting;
    resource = make_shared< Resource >();
    statusResource = make_shared< Resource >();
    restbed_settings = make_shared< Settings >();

}

void TTS_Server::start() {
    reqnum = 0;
    auto callback = bind(&TTS_Server::postMethodHandler, this, placeholders::_1);
    tradeApi = make_shared<TTS_TradeApi>(_setting.trade_dll_path);
    resource->set_path("/api");
    resource->set_method_handler("POST", callback);

    statusResource->set_path("/status");
    statusResource->set_method_handler("GET", [&](const shared_ptr< Session > session){
        const auto request = session->get_request();
        json j;
        j["success"] = true;
        j["reqnum"] = reqnum;
        session->close(OK, j.dump());
    });

    restbed_settings->set_port(_setting.port);
    restbed_settings->set_default_header("Connection", "close");

    service.publish(resource);
    service.publish(statusResource);
    qInfo() << "Starting to listening.." ;
    service.start(restbed_settings);

}


void TTS_Server::stop() {
    qInfo() << "GoodByte!";
    service.stop();
}

/**
 * @brief TTS_Server::postMethodHandler
 *
 * 这里，请求体的结构应为
 * {
 *      "func": "Logon",
 *      "params": {
 *          ..
 *          ..
 *      }
 * }
 *
 * @param session
 */
void TTS_Server::postMethodHandler(const shared_ptr< Session > session) {
    const auto request = session->get_request();
    int contentLength = request->get_header("Content-Length", 0);

    session->fetch(contentLength, [&] (const shared_ptr<Session> session, const Bytes& body) {
        string requestBody(body.begin(), body.end());
        reqnum++;
        json requestJson = json::parse(requestBody);

        if (requestJson["func"].is_null()) {
            QString _err= "parameter func does not exists";
            session->close(OK, tradeApi->jsonError(_err).dump());
            return;
        }

        string func = requestJson["func"].get<string>();
        qInfo("Receiving request func=%s", func.c_str());
        string responseBody;
        auto params = requestJson["params"];
        // 参数的解析，后续应该用Command等模式将实现放到具体的类中
        if (func == P_LOGON) {
            if (params["ip"].is_string() && params["port"].is_number() && params["version"].is_string()
                    && params["yyb_id"].is_number() && params["account_no"].is_string()
                    && params["trade_account"].is_string()
                    && params["jy_password"].is_string() && params["tx_password"].is_string()) {
                string ip = params["ip"].get<string>();
                int port = params["port"].get<int>();
                string version = params["version"].get<string>();
                int yybId = params["yyb_id"].get<int>();
                string accountNo = params["account_no"].get<string>();
                string tradeAccount = params["trade_account"].get<string>();
                string jyPassword = params["jy_password"].get<string>();
                string txPassword = params["tx_password"].get<string>();

                responseBody = tradeApi->logon(ip.c_str(), port, version.c_str(), yybId, accountNo.c_str(), tradeAccount.c_str(), jyPassword.c_str(), txPassword.c_str()).dump();
            } else {
                responseBody = tradeApi->jsonError("error params").dump();
            }

        } else if (func == P_LOGOFF) {
            if (params["client_id"].is_number()) {
                responseBody = tradeApi->logoff(params["client_id"].get<int>()).dump();
            } else {
                responseBody = tradeApi->jsonError("error params").dump();
            }
        } else if (func == P_QUERYDATA) {
            if (params["client_id"].is_number()
                    && params["category"]
                    ) {
                responseBody = tradeApi->queryData(params["client_id"].get<int>(), params["category"].get<int>());
            } else {
                responseBody = tradeApi->jsonError("error params").dump();
            }
        } else if (func == P_SENDORDER) {
            if (params["client_id"].is_number()
                    && params["category"].is_number()
                    && params["price_type"].is_number()
                    && params["gddm"].is_string()
                    && params["zqdm"].is_string()
                    && params["price"].is_number()
                    && params["quantity"].is_number()) {
                responseBody = tradeApi->sendOrder(
                            params["client_id"].get<int>(),
                            params["category"].get<int>(),
                        params["price_type"].get<int>(),
                        params["gddm"].get<string>().c_str(),
                        params["zqdm"].get<string>().c_str(),
                        params["price"].get<float>(),
                        params["quantity"].get<int>()
                            ).dump();
            } else {
                responseBody = tradeApi->jsonError("error params").dump();
            }
        } else if (func == P_GETQUOTE || func == P_CANCELORDER)
            /** 刚刚好这两个func的参数一致的 **/
        {
            if (params["client_id"].is_number()
                    && params["exchange_id"].is_string()
                    && params["hth"].is_string()) {
                responseBody = tradeApi->getQuote(
                            params["client_id"].get<int>(),
                        params["exchange_id"].get<string>().c_str(),
                        params["hth"].get<string>().c_str()
                            ).dump();
            } else {
                responseBody = tradeApi->jsonError("error params").dump();
            }
        } else if (func == P_REPAY) {
            if (params["client_id"].is_number()
                    && params["amount"].is_string()) {
                responseBody = tradeApi->repay(params["client_id"].get<int>(), params["amount"].get<string>().c_str()).dump();
            } else {
                responseBody = tradeApi->jsonError("error params").dump();
            }
        } else if (func == "stop_server") {
            qInfo() << "Server Stop Command Called!";
            stop();
        } else {
            responseBody = "{\"success\":false, \"error\": \"unknown command\"}";
        }

        session->close(OK, responseBody);
    });
}
