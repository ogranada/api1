#include <iostream>
#include <served/served.hpp>
#include "Poco/MongoDB/MongoDB.h"
#include "Poco/MongoDB/Connection.h"
#include "Poco/MongoDB/Database.h"
#include "Poco/MongoDB/Cursor.h"
#include "Poco/MongoDB/Array.h"
#include "json/json.hpp"


served::multiplexer mux;
Poco::MongoDB::Connection connection("localhost", 27017);

namespace models {
  struct person {
    std::string name;
  };

  void to_json(nlohmann::json& j, const person& p) {
      j = nlohmann::json{{"name", p.name}};
  }

  void from_json(const nlohmann::json& j, person& p) {
      p.name = j["name"].get<std::string>();
  }
}

void list_users(served::response &res, const served::request &req) {
  res.set_header("Content-Type", "application/json; charset=utf-8");
  Poco::MongoDB::Cursor cursor("cppsample", "elements");
  Poco::MongoDB::ResponseMessage& response = cursor.next(connection);
  std::vector<nlohmann::json> v;
  std::string filter = req.query["name"];
  for (Poco::MongoDB::Document::Vector::const_iterator it = response.documents().begin(); it != response.documents().end(); ++it)
  {
    models::person p = {(*it)->get<std::string>("name")};
    if(filter.length()>0){
      if (p.name.find(filter) != std::string::npos) {
        nlohmann::json js;
        models::to_json(js, p);
        v.push_back( js );
      }
    } else {
      nlohmann::json js;
      models::to_json(js, p);
      v.push_back( js );
    }
  }
  nlohmann::json rta(v);
  res << rta.dump() ;
}

int main() {
  mux.handle("/")
  .get([](served::response &res, const served::request &req) {
    list_users(res, req);
  })
  .post([](served::response &res, const served::request &req) {
    if (req.header("Content-Type") != "application/json") {
      served::response::stock_reply(400, res);
      return;
    }
    std::string body = req.body();
    auto jsonBody = nlohmann::json::parse(body);
    if (jsonBody.count("name") > 0) {
      Poco::MongoDB::Database db("cppsample");
      Poco::SharedPtr<Poco::MongoDB::InsertRequest> insertElm = db.createInsertRequest("elements");
      std::string name = jsonBody["name"];
      insertElm->addNewDocument()
         .add("name", name);
      connection.sendRequest(*insertElm);
      std::string lastError = db.getLastError(connection);
      if (!lastError.empty())
      {
        std::cout << "Last Error: " << db.getLastError(connection) << std::endl;
        res << "{\"status\": \"fail\", \"message\":\"" << lastError << "\"}";
      } else {
        res << "{\"status\": \"success\", \"message\":\"inserted\"}";
      }
    } else {
      res.set_status(400);
      res.set_header("Content-Type", "application/json");
      res << "{\"status\": \"fail\", \"message\":\"invalid name\"}";
    }
  })
  ;
  served::net::server server("127.0.0.1", "8080", mux);
  std::cout << "Listening... " << std::endl;
  server.run(10);
  
  return 0;
}
