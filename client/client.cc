// grpc libraries
#include <grpcpp/grpcpp.h>
#include "mafia.grpc.pb.h"

#include <thread>
#include <unordered_set>
// common grpc structures
using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using mafia::mafiaService;
using mafia::nick;
using mafia::displayUser;
using mafia::gameStart;
using mafia::voteResult;
using mafia::vote;
using mafia::gameResult;
using mafia::empty;
using mafia::gameRequest;
using mafia::roleRequest;

class ClientService {
public:
    ClientService(std::shared_ptr<Channel> channel) : stub_(mafiaService::NewStub(channel)) {
        std::cin >> Nick;
        nick request;
        request.set_nick_name(Nick);
        ClientContext context;
        gameRequest response;
        auto status = stub_->setNickName(&context, request, &response);
        game = response.game();
        std::thread([&]() {
            gameRequest req;
            req.set_game(response.game());
            ClientContext context;
            std::shared_ptr<grpc::ClientReader<displayUser> > stream(
                    stub_->getUsers(&context, req));
            displayUser user;
            while(stream->Read(&user)) {
                std::lock_guard<std::mutex> lock(player_mutex_);
                players_.insert(user.user_name());
                system("cls");
                for(const auto& player: players_) {
                    std::cout << player << std::endl;
                }
            }
        });
        std::thread([&]() {
            gameRequest req;
            req.set_game(response.game());
            ClientContext context;
            std::shared_ptr<grpc::ClientReader<displayUser> > stream(
                    stub_->getUsers(&context, req));
            displayUser user;
            while(stream->Read(&user)) {
                std::lock_guard<std::mutex> lock(player_mutex_);
                players_.erase(user.user_name());
                system("cls");
                for(const auto& player: players_) {
                    std::cout << player << std::endl;
                }
            }
        });

        nick n;
        mafia::status s;
        n.set_nick_name(Nick);
        status = stub_->getState(&context, n, &s);
        role_ = s.role();

        if(role_ == "mafia") {
            vote msg;
            empty e;
            msg.set_player(rand() % 10);
            msg.set_game(game);
            stub_->kill(&context, msg, &e);
        } else {
            vote msg;
            empty e;
            msg.set_player(rand() % 10);
            msg.set_game(game);
            stub_->makeVote(&context, msg, &e);
        }
    }
private:
    std::unique_ptr<mafiaService::Stub> stub_;
    std::unordered_set<std::string> players_;
    std::mutex player_mutex_;
    std::string role_;
    std::string Nick;
    int game;
};

void RunClient(const std::string& server_address) {
    ClientService client(grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials()));
}

int main() {
    RunClient("0.0.0.0:50052");
    return 0;
}