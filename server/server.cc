#include <thread>

// grpc libraries
#include <grpcpp/grpcpp.h>
#include <condition_variable>
#include "mafia.grpc.pb.h"

// common grpc structures
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using grpc::ServerWriter;

// structures and services
using mafia::mafiaService;
using mafia::nick;
using mafia::displayUser;
using mafia::gameStart;
using mafia::status;
using mafia::voteResult;
using mafia::vote;
using mafia::gameResult;
using mafia::empty;
using mafia::gameRequest;
using mafia::roleRequest;

class ServerService final : public mafiaService::Service {
    Status setNickName(ServerContext* context, const nick* request, gameRequest* response) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        queue_.insert(request->nick_name());
        if(queue_.size() == 10) {
            games_.emplace_back(queue_);
            queue_.clear();
            game_notifier_.notify_all();
        } else {
            game_notifier_.wait(lock);
        }
        return Status::OK;
    }

    Status getUsers(ServerContext* context, const gameRequest* request,
                    ServerWriter<displayUser>* writer) {
        for(const auto& nick: games_[request->game()]) {
            displayUser msg;
            msg.set_user_name(nick);
            writer->Write(msg);
        }
        return Status::OK;
    }

    Status deleteUser(ServerContext* context, const gameRequest* request,
                      ServerWriter<displayUser>* writer) {
        return Status::OK;

    }

    Status getState(ServerContext* context, const roleRequest* request, status* response) {
        if(roles_.find(request->nick_name()) == roles_.end()) {
            size_t counter = 0;
            for(const auto& name: games_[request->game()]) {
                if(counter < 3) {
                    roles_[name] = "Mafia";
                    ++counter;
                } else {
                    roles_[name] = "Peaceful";
                }
            }
        }
        response->set_role(roles_[request->nick_name()]);
        return Status::OK;
    }

    Status kill(ServerContext* context, const vote* request, empty* response) {
        auto it = games_[request->game()].begin();
        uint32_t i = 0;
        while(i != request->player()) {
            ++i;
            it = std::next(it);
        }
        games_[request->game()].erase(it);
        return Status::OK;
    }

    Status makeVote(ServerContext* context, const vote* request, empty* response) {
        auto it = games_[request->game()].begin();
        uint32_t i = 0;
        while(i != request->player()) {
            ++i;
            it = std::next(it);
        }
        games_[request->game()].erase(it);
        return Status::OK;
    }

    Status endNotifier(ServerContext* context, const nick* request, gameResult* response) {
        return Status::OK;
    }
private:
    std::mutex queue_mutex_;
    std::condition_variable game_notifier_;
    std::mutex game_notifier_locker_;
    std::unordered_set<std::string> queue_;
    std::unordered_map<std::string, std::string> roles_;
    std::vector<std::unordered_set<std::string>> games_;
    std::vector<std::thread> threads_;
};

void RunServer(const std::string& address) {
    ServerService service;
    ServerBuilder builder;
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Serer listening on: " << address << std::endl;
    server->Wait();
}

int main() {
    std::string address = "0.0.0.0:50052";
    RunServer(address);
    return 0;
}