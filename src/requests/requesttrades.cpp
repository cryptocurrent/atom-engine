#include "requesttrades.h"

#include <hiredis/async.h>

CRequestTrades::CRequestTrades(redisAsyncContext* redisContext, evhttp_request* req, TDocumentUniquePtr document, OnRequestProcessingCompleteFunc onComplete) :
	CRequest(redisContext, req, std::move(document), onComplete),
	isFirstKey_(true)
{

}

CRequestTrades::~CRequestTrades()
{

}

void CRequestTrades::process()
{
	if (!document_->HasMember("first_cur") || !document_->HasMember("second_cur")) {
		sendBadJSONError();
		if (onComplete_) {
			onComplete_(this);
		}
	} else {
		std::string firstCur = (*document_)["first_cur"].GetString();
		std::string secondCur = (*document_)["second_cur"].GetString();

		std::stringstream key;
		std::stringstream command;
		key << "trades:" << firstCur << "_" << secondCur << ":*";
		command << "keys " << key.str();

		redisAsyncCommand(redisContext_, &CRequestTrades::getCallback, this, command.str().c_str());
	}
}

std::string CRequestTrades::getType()
{
	return "trades";
}

void CRequestTrades::getCallback(redisAsyncContext* context, void* res, void* data)
{
	redisReply* value = reinterpret_cast<redisReply*>(res);
	CRequestTrades* request = reinterpret_cast<CRequestTrades*>(data);
	if (value) {
		for (size_t i = 0; i < value->elements; ++i) {
			std::string str = value->element[i]->str;
			request->keys_.push_back(str);
		}
		if (request->keys_.empty()) {
			request->sendReply("{[]}");
			if (request->onComplete_) {
				request->onComplete_(request);
			}
		} else {
			request->res_ << "{[";
			request->getNextValue();
		}
	} else {
		request->sendReply("{[]}");
		if (request->onComplete_) {
			request->onComplete_(request);
		}
	}
}

void CRequestTrades::getValueCallback(redisAsyncContext* context, void* res, void* data)
{
	redisReply* value = reinterpret_cast<redisReply*>(res);
	CRequestTrades* request = reinterpret_cast<CRequestTrades*>(data);
	if (request->keys_.empty()) {
		request->res_ << "]}";
		request->sendReply(request->res_.str());
		if (request->onComplete_) {
			request->onComplete_(request);
		}
	} else {
		rapidjson::Document doc;
		if (!doc.Parse(value->str).HasParseError() && doc.HasMember("first_count") && doc.HasMember("second_count")) {
			std::stringstream ss;
			ss << "{\"key\": " << request->curKey_ << ", \"first_count\": " << doc["first_count"].GetInt() << ", \"second_count\": " << doc["second_count"].GetInt() << "}";
			if (!request->isFirstKey_) {
				request->res_ << ", ";
			}
			request->isFirstKey_ = false;	
			request->res_ << ss.str();
		}
		request->getNextValue();
	}
}

void CRequestTrades::getNextValue()
{
	auto it = keys_.begin();
	curKey_ = *it;
	keys_.erase(it);
	std::stringstream command;
	command << "GET " << curKey_;
	redisAsyncCommand(redisContext_, &CRequestTrades::getValueCallback, this, command.str().c_str());
}