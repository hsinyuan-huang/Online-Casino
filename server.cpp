// Assume # of users will not > 100 people simultaneously
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <algorithm>
#include <mutex>
#include <thread>
#include <queue>
#include <vector>

void runUser(int uid);

class User{
public:
	int soc;
	int bet;
	int money;
	int round;
}Users[100];

class Game{
public:
	int num_player;
	int num_finish;
	int uid[5];
	int uniq[4];
	std::mutex mtx;
	
	int which;
	int mx_player;
}Games[3][5][3];

std::mutex mtx_avaU;
std::queue<int> avaU;

int socket_getint(int fd){
	int cnt = 0;
	char buf[100];
	while(!(cnt > 0 && buf[cnt - 1] == '\n')){
		read(fd, buf + cnt, 1);
		buf[++cnt] = 0;
	}

	int a_int;
	sscanf(buf, "%d", &a_int);
	return a_int;
}

void socket_sendint(int fd, int a_int){
	char buf[100];
	sprintf(buf, "%d\n", a_int);
	write(fd, buf, strlen(buf));
}

void GenUnique4(int uniq[4]){
	for(int i = 0; i < 4; i++){
		bool flag = 0;
		while(!flag){
			uniq[i] = rand() % 10;
			
			bool all_diff = 1;
			for(int j = 0; j < i; j++){
				if(uniq[i] == uniq[j]){
					all_diff = 0;
					break;
				}
			}

			flag = all_diff;
		}
	}
}

double betBX[5] = {0, 1, 1.5, 2, 2.5};
double betXX[5] = {0, 1, 0.4, 0.2, 0};

void PlayGame1(int gt, int pl, int rm){
	Game &G = Games[gt][pl][rm];
	
	G.mtx.lock();

	int lost = 0, bestR = 1000;
	for(int i = 0; i < pl; i++){
		if(Users[G.uid[i]].round != -1) bestR = std::min(bestR, Users[G.uid[i]].round);
		else lost ++;
	}

	for(int i = 0; i < pl; i++){
		User &U = Users[G.uid[i]];
		
		socket_sendint(U.soc, lost);
		if(lost < pl) socket_sendint(U.soc, bestR);

		if(U.round == -1){
			socket_sendint(U.soc, -U.bet);
			U.money += -U.bet;
		}
		else{
			if(U.round == bestR){
				socket_sendint(U.soc, (int)(betBX[pl] * U.bet));
				U.money += (int)(betBX[pl] * U.bet);
			}
			else{
				socket_sendint(U.soc, (int)(betXX[pl] * U.bet));
				U.money += (int)(betXX[pl] * U.bet);
			}
		}
	}
	
	G.num_player = 0;
	G.num_finish = 0;
	GenUnique4(G.uniq);
	for(int i = 0; i < pl; i++)
		std::thread (runUser, Games[gt][pl][rm].uid[i]).detach();
	G.mtx.unlock();
}

class deck{
public:
	// INTEGER % 4: Suit, INTEGER / 4: Value
	deck(){
		for(int i = 0; i < 4; i++){
			for(int j = 0; j < 52; j++){
				shoe[i * 52 + j] = j;
			}
		}
	}
	void shuffle(){
		for(int i = 0; i < 52 * 4; i++){
			int j = rand() % (52 * 4);
			std::swap(shoe[i], shoe[j]);
		}
		top = 0;
	}
	int draw(){
		if(top == 52 * 4)
			shuffle();
		return shoe[top ++];
	}
private:
	int top;
	int shoe[52 * 4];
};

class pokerhand{
public:
	pokerhand(){}
	pokerhand(int A, int B){
		nhands = 1;
		
		hands[0].clear();
		hands[0].push_back(A);
		hands[0].push_back(B);

		doublebet[0] = 0;
	}
	int nhands, doublebet[5];
	std::vector<int> hands[5];
};

void sends_cards_to_all(int nplayer, int uid[], pokerhand phands[]){
	for(int i = 0; i < nplayer; i++){
		int sockfd = Users[uid[i]].soc;

		for(int j = 0; j < nplayer + 1; j++){
			socket_sendint(sockfd, phands[j].nhands); // Number of hands

			for(int k = 0; k < phands[j].nhands; k++){
				socket_sendint(sockfd, phands[j].doublebet[k]); // Double bet
				socket_sendint(sockfd, (int)phands[j].hands[k].size()); // Hands Length
				for(auto poker : phands[j].hands[k]){
					socket_sendint(sockfd, poker);
				}
			}
		}
	}
}

// Tell all player an integer
void notify_all(int nplayer, int uid[], int val){
	for(int i = 0; i < nplayer; i++){
		int sockfd = Users[uid[i]].soc;
		socket_sendint(sockfd, val);
	}
}

int calc_value(std::vector<int> &hand){
	int ace = 0, total = 0;
	
	for(auto poker : hand){
		int val = poker / 4; val ++;
		
		if(val == 1) ace ++;
		else if(val >= 10) total += 10;
		else total += val;
	}

	total += ace;
	for(int i = 0; i < ace; i++)
		if(total + 10 <= 21) total += 10;

	return total;
}

int level(int val, int ncard, bool isdealer){
	if(val > 21){
		if(isdealer) return -5; // when dealer and player both busted, dealer win
		else return -10;
	}
	if(val == 21 && ncard == 2) return 100;
	return val;
}

void PlayGame2(int gt, int pl, int rm){
	Game &G = Games[gt][pl][rm];
	
	G.mtx.lock();
	// Using 4 decks
	deck myshoe;
	myshoe.shuffle();

	pokerhand phands[5]; // 4 players + 1 dealer
	for(int i = 0; i < pl + 1; i++)
		phands[i] = pokerhand(myshoe.draw(), myshoe.draw());

	for(int i = 0; i < pl; i++){
		int sockfd = Users[G.uid[i]].soc;
		// You are player i
		socket_sendint(sockfd, i);
	}
	
	for(int i = 0; i < pl; i++){
		int sockfd = Users[G.uid[i]].soc;

		for(int j = 0; j < phands[i].nhands; j++){
			notify_all(pl, G.uid, i);
			notify_all(pl, G.uid, j);
			sends_cards_to_all(pl, G.uid, phands);
			notify_all(pl, G.uid, 1); // Side Rule or Main Rule

			socket_sendint(sockfd, (int)phands[i].hands[j].size());
			for(auto poker : phands[i].hands[j])
				socket_sendint(sockfd, poker);
			
			int action = socket_getint(sockfd);
			
			if(action == -1){ // No side rules
				notify_all(pl, G.uid, -1);
			}
			if(action == 0){ // Split
				int poker = phands[i].hands[j][0];
				phands[i].hands[j].pop_back();
				phands[i].hands[j].push_back(myshoe.draw());

				phands[i].hands[phands[i].nhands].clear();
				phands[i].hands[phands[i].nhands].push_back(poker);
				phands[i].hands[phands[i].nhands].push_back(myshoe.draw());
				
				phands[i].doublebet[phands[i].nhands] = 0;
				phands[i].nhands ++;

				notify_all(pl, G.uid, 0);

				j--;
				continue;
			}
			if(action == 1){ // Double
				phands[i].doublebet[j] = 1;
				phands[i].hands[j].push_back(myshoe.draw());

				if(calc_value(phands[i].hands[j]) <= 21){ // Double and End
					notify_all(pl, G.uid, 1);
				}
				else{ // Double and Busted
					notify_all(pl, G.uid, 2);
				}
				continue;
			}
			
			std::vector<int> &cur_hand = phands[i].hands[j];
			while(1){
				notify_all(pl, G.uid, i);
				notify_all(pl, G.uid, j);
				sends_cards_to_all(pl, G.uid, phands);
				notify_all(pl, G.uid, 2); // Side Rule or Main Rule

				socket_sendint(sockfd, (int)cur_hand.size());
				for(auto poker : cur_hand)
					socket_sendint(sockfd, poker);

				action = socket_getint(sockfd);
				
				if(action == 0){ // Stand
					// Tell all, he stand
					notify_all(pl, G.uid, 0);
					break;
				}
				if(action == 1){ // Hit
					cur_hand.push_back(myshoe.draw());
					
					if(calc_value(cur_hand) > 21){
						// Tell all, he hit and busted
						notify_all(pl, G.uid, 2);
						break;
					}
					
					// Tell all, he hit
					notify_all(pl, G.uid, 1);
				}
			}
		}
	}
	notify_all(pl, G.uid, -10000);
	sends_cards_to_all(pl, G.uid, phands);

	while(calc_value(phands[pl].hands[0]) < 17)
		phands[pl].hands[0].push_back(myshoe.draw());
	sends_cards_to_all(pl, G.uid, phands);

	int earn[5];

	for(int i = 0; i < pl; i++){
		notify_all(pl, G.uid, phands[i].nhands);

		earn[i] = 0;
		
		for(int j = 0; j < phands[i].nhands; j++){
			int val = calc_value(phands[i].hands[j]);
			int Dval = calc_value(phands[pl].hands[0]);

			int lvl = level(val, (int)phands[i].hands[j].size(), 0);
			int Dlvl = level(Dval, (int)phands[pl].hands[0].size(), 1);

			if(lvl < Dlvl){
				if(lvl == -10) notify_all(pl, G.uid, -2); // bust
				else notify_all(pl, G.uid, -1); // lost

				earn[i] -= Users[G.uid[i]].bet * (phands[i].doublebet[j]? 2: 1);
			}
			if(lvl == Dlvl){
				notify_all(pl, G.uid, 0); // push
			}
			if(lvl > Dlvl){
				if(lvl == 100){
					if(phands[i].hands[j][0] % 4 == 0 && phands[i].hands[j][1] % 4 == 0
					&& (phands[i].hands[j][0] / 4 == 11 || phands[i].hands[j][1] / 4 == 11)){
						notify_all(pl, G.uid, 1000); // Royal BlackJack
						earn[i] += 5 * Users[G.uid[i]].bet * (phands[i].doublebet[j]? 2: 1);
					}
					else{
						notify_all(pl, G.uid, 100); // BlackJack
						earn[i] += 3 * Users[G.uid[i]].bet * (phands[i].doublebet[j]? 2: 1) / 2;
					}
				}
				else{
					if((int)phands[i].hands[j].size() >= 5){
						notify_all(pl, G.uid, 50); // Dragon
						earn[i] += 3 * Users[G.uid[i]].bet * (phands[i].doublebet[j]? 2: 1);
					}
					else{
						notify_all(pl, G.uid, 1); // win
						earn[i] += Users[G.uid[i]].bet * (phands[i].doublebet[j]? 2: 1);
					}
				}
			}
		}

		Users[G.uid[i]].money += earn[i];
	}

	for(int i = 0; i < pl; i++)
		socket_sendint(Users[G.uid[i]].soc, earn[i]);

	G.num_player = 0;
	G.num_finish = 0;
	GenUnique4(G.uniq);
	for(int i = 0; i < pl; i++)
		std::thread (runUser, Games[gt][pl][rm].uid[i]).detach();
	G.mtx.unlock();
}

void runUser(int uid){
	User &cu = Users[uid];
	socket_sendint(cu.soc, cu.money);

	if(cu.money < 10){
		fprintf(stderr, "User %d leaving the game system\n", uid);
		close(cu.soc);
	
		mtx_avaU.lock();
		avaU.push(uid);
		mtx_avaU.unlock();
		return;
	}

	while(1){
		int gametype = socket_getint(cu.soc);
		fprintf(stderr, "User %d selects Game Type: %d\n", uid, gametype);
	
		if(gametype == -1){
			fprintf(stderr, "User %d leaving the game system\n", uid);
			close(cu.soc);
	
			mtx_avaU.lock();
			avaU.push(uid);
			mtx_avaU.unlock();
			return;
		}

		int playerN, gameroom;
		bool gameroom_selected = 0;
		
		while(!gameroom_selected){
			for(int i = 1; i <= 4; i++){
				for(int j = 0; j < 3; j++){
					Games[gametype][i][j].mtx.lock();
					socket_sendint(cu.soc, Games[gametype][i][j].num_player);
					Games[gametype][i][j].mtx.unlock();
				}
			}
		
			playerN = socket_getint(cu.soc);
			gameroom = socket_getint(cu.soc);
			if(!(playerN >= 1 && playerN <= 4 && gameroom >= 0 && gameroom <= 2)){
				fprintf(stderr, "User %d going back to game selection\n", uid);
				break;
			}
			fprintf(stderr, "User %d wants to enter %d-player Game Room %c\n", uid, playerN, gameroom + 'A');
	
			Games[gametype][playerN][gameroom].mtx.lock();
			if(Games[gametype][playerN][gameroom].num_player < playerN){
				socket_sendint(cu.soc, 1);
				int &tmp = Games[gametype][playerN][gameroom].num_player;
				Games[gametype][playerN][gameroom].uid[tmp ++] = uid;
				
				gameroom_selected = 1;
			}
			else socket_sendint(cu.soc, -1);
			Games[gametype][playerN][gameroom].mtx.unlock();
		}

		if(gameroom_selected){
			if(gametype == 1){
				cu.bet = socket_getint(cu.soc);
				fprintf(stderr, "User %d submit bet $%d\n", uid, cu.bet);

				int *ans = Games[gametype][playerN][gameroom].uniq;
				fprintf(stderr, "User %d starts to guess, ans=%d%d%d%d\n", uid, ans[0], ans[1], ans[2], ans[3]);

				cu.round = -1;
				for(int i = 0; i < 10; i++){
					int guess[4];
					for(int j = 0; j < 4; j++)
						guess[j] = socket_getint(cu.soc);
					
					int xA = 0, yB = 0;
					for(int j = 0; j < 4; j++)
						if(ans[j] == guess[j]) xA++;
					for(int j = 0; j < 4; j++)
						for(int k = 0; k < 4; k++)
							if(ans[j] == guess[k]) yB++; // This is actually xA + yB
					yB -= xA;

					socket_sendint(cu.soc, xA);
					socket_sendint(cu.soc, yB);

					if(xA == 4 && yB == 0){
						cu.round = i + 1;
						fprintf(stderr, "User %d correctly guessed the answer in %d rounds\n", uid, cu.round);
						break;
					}
				}

				bool last_finished = 0;
				Games[gametype][playerN][gameroom].mtx.lock();
				Games[gametype][playerN][gameroom].num_finish ++;
				if(Games[gametype][playerN][gameroom].num_finish == playerN)
					last_finished = 1;
				Games[gametype][playerN][gameroom].mtx.unlock();

				if(last_finished)
					std::thread (PlayGame1, gametype, playerN, gameroom).detach();
				return;
			}
			else if(gametype == 2){
				cu.bet = socket_getint(cu.soc);
				fprintf(stderr, "User %d submit bet $%d\n", uid, cu.bet);

				socket_getint(cu.soc); //DEAL!
				
				bool last_dealed = 0;
				Games[gametype][playerN][gameroom].mtx.lock();
				Games[gametype][playerN][gameroom].num_finish ++;
				if(Games[gametype][playerN][gameroom].num_finish == playerN)
					last_dealed = 1;
				Games[gametype][playerN][gameroom].mtx.unlock();
				
				if(last_dealed)
					std::thread (PlayGame2, gametype, playerN, gameroom).detach();
				return;
			}
			else fprintf(stderr, "ERROR: This is impossible\n");
		}
	}
}

int main(int argv, char* argc[]){
	if(argv != 2){
		fprintf(stderr, "Usage: ./server port\n");
		exit(-1);
	}

	srand(clock());

	int listenfd = socket(AF_INET, SOCK_STREAM, 0);
	
	struct sockaddr_in serverAddress;
	bzero(&serverAddress, sizeof(serverAddress));
	
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
	serverAddress.sin_port = htons(atoi(argc[1]));
	
	if(bind(listenfd, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0)
		fprintf(stderr, "bind failed!\n");
	if(listen(listenfd, 100) < 0)
		fprintf(stderr, "listen failed\n");

	for(int i = 0; i < 100; i++)
		avaU.push(i);

	for(int k = 0; k < 2; k++){
		for(int i = 1; i <= 4; i++){
			for(int j = 0; j < 3; j++){
				Games[k][i][j].mtx.lock();
				Games[k][i][j].which = k;
				Games[k][i][j].mx_player = i;
				
				Games[k][i][j].num_player = 0;
				Games[k][i][j].num_finish = 0;
				GenUnique4(Games[k][i][j].uniq);
				Games[k][i][j].mtx.unlock();
			}
		}
	}

	while(1){
		struct sockaddr_in clientAddress;
		socklen_t length = sizeof(clientAddress);
		int connfd = accept(listenfd, (struct sockaddr*)&clientAddress, &length);

		mtx_avaU.lock();
		int uid = avaU.front();
		avaU.pop();
		mtx_avaU.unlock();
		
		fprintf(stderr, "Receive 1 player: User %d\n", uid);
		
		Users[uid].soc = connfd;
		Users[uid].money = 300;
		std::thread (runUser, uid).detach();
	}
}
