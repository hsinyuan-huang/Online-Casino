#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <vector>
#include <algorithm>
using namespace std;

void GameMenu();

int sockfd, curMoney;
int play, room;

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

bool valid_uniq(int guess[4]){
	for(int i = 0; i < 4; i++){
		if(guess[i] < 0 || guess[i] > 9) return false;
		for(int j = 0; j < i; j++)
			if(guess[i] == guess[j]) return false;
	}
	return true;
}

double betBX[5] = {0, 1, 1.5, 2, 2.5};
double betXX[5] = {0, 1, 0.4, 0.2, 0};

void StartGame1(){ // Guess Number
	printf("\nGame Rule:\n");
	printf("    Enter 4 unique digits (0~9, e.g., 1032 or 1982) each round,\n");
	printf("    (The 4 digits need to be entered consecutively, without space)\n");
	printf("    We will give you \"x A y B\"\n");
	printf("    which means there are x: digit & position are both correct,\n");
	printf("            and there are y: only digit is correct.\n");
	printf("    You have total chance of 10 rounds to guess the number (win), otherwise (lose)\n");
	printf("    If you lose, you lost your bet.\n");
	printf("    If you win, and is the best of your room, you get %.1fx your bet\n", betBX[play]);
	printf("    If you win, but is not the best of your room, you get %.1fx your bet\n", betXX[play]);

	printf("Please enter your bet (10, 20 to %d): ", (curMoney/10) * 10);

	char buff[100];
	fgets(buff, 100, stdin);
	int bet = -1;
	int ret = sscanf(buff, "%d", &bet);
	if(ret != 1) bet = -1;

	while(!(bet % 10 == 0 && bet >= 10 && bet <= curMoney)){
		printf("Invalid bet, Re-enter: ");
		
		fgets(buff, 100, stdin);
		bet = -1;
		ret = sscanf(buff, "%d", &bet);
		if(ret != 1) bet = -1;
	}
	socket_sendint(sockfd, bet);

	int win = 0;
	printf("\nGame Starting ...\n");
	sleep(1);

	int round = -1;
	for(int i = 0; i < 10; i++){
		printf("Round %d: ", i+1);

		int guess[4];
		fgets(buff, 100, stdin);
		if(!(strlen(buff) == 5 && buff[4] == '\n'))
			guess[0] = guess[1] = guess[2] = guess[3] = 0;
		else{
			for(int j = 0; j < 4; j++){
				if(!(buff[j] >= '0' && buff[j] <= '9')){
					guess[0] = guess[1] = guess[2] = guess[3] = 0;
					break;
				}
				guess[j] = buff[j] - '0';
			}
		}

		while(!valid_uniq(guess)){
			printf("Wrong input, Re-enter: ");
			
			fgets(buff, 100, stdin);
			if(!(strlen(buff) == 5 && buff[4] == '\n'))
				guess[0] = guess[1] = guess[2] = guess[3] = 0;
			else{
				for(int j = 0; j < 4; j++){
					if(!(buff[j] >= '0' && buff[j] <= '9')){
						guess[0] = guess[1] = guess[2] = guess[3] = 0;
						break;
					}
					guess[j] = buff[j] - '0';
				}
			}
		}

		for(int j = 0; j < 4; j++)
			socket_sendint(sockfd, guess[j]);
		int xA = socket_getint(sockfd);
		int yB = socket_getint(sockfd);

		printf("Guess Result: %dA%dB\n", xA, yB);

		if(xA == 4 && yB == 0){
			win = 1;
			round = i + 1;
			printf("Congratulation! You correctly guess the number in %d rounds!\n", i + 1);
			break;
		}
	}
	if(!win) printf("Sorry...you lost...QQ\n");

	if(play > 1)
		printf("Waiting for other player to finish their game...\n");

	int num_lost = socket_getint(sockfd);
	printf("Total %d players, %d has lost.\n", play, num_lost);
	if(num_lost < play){
		int bestR = socket_getint(sockfd);
		printf("The best player guessed the answer in %d rounds.\n", bestR);
		if(bestR == round) printf("Wow!! You are the best player in this room!\n");
	}
	else printf("Everyone has failed ._.\n");

	int earn = socket_getint(sockfd);

	printf("\nCalculating your earning ...\n");
	sleep(1);

	if(earn > 0) printf("You won $%d! :)\n", earn);
	else printf("You lost %d dollar! :(\n", -earn);

	curMoney = socket_getint(sockfd);
	printf("You now have %d dollar\n", curMoney);

	printf("\nGoing back to Game Menu ...\n");
	sleep(1);
}

int poker_value(int poker){
	int v = poker / 4 + 1;
	if(v >= 10) return 10;
	return v;
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

char suit[4] = {'S', 'H', 'D', 'C'};
char face[13] = {'A', '2', '3', '4', '5', '6', '7', '8', '9', 'T', 'J', 'Q', 'K'};

void StartGame2(){ // Black Jack
	printf("\nGame Rule:\n");
	printf("    Double down available: When you have 2 cards\n");
	printf("    Split available: When you have 2 cards & same value\n");
	printf("    Double down after Split is permitted\n");
	printf("    Dealer stands on soft 17\n");
	printf("    If you win, you may have some additional bonus based on your poker hand\n");
	printf("Please enter your bet (10, 20 to %d): ", (curMoney/10) * 10);
	
	char buff[100];
	fgets(buff, 100, stdin);
	int bet = -1;
	int ret = sscanf(buff, "%d", &bet);
	if(ret != 1) bet = -1;

	while(!(bet % 10 == 0 && bet >= 10 && bet <= curMoney)){
		printf("Invalid bet, Re-enter: ");
		
		fgets(buff, 100, stdin);
		bet = -1;
		ret = sscanf(buff, "%d", &bet);
		if(ret != 1) bet = -1;
	}
	socket_sendint(sockfd, bet);
	curMoney -= bet;

	printf("\nGame Starting ...\n");
	sleep(1);

	printf("Type [D]eal to deal cards: ");
	char deal[100];
	fgets(deal, 100, stdin);

	while(!(deal[0] == 'D')){
		printf("Wrong input, Re-enter: ");
		fgets(deal, 100, stdin);
	}
	socket_sendint(sockfd, 1);

	if(play > 1)
		printf("Waiting for other player to deal...\n");

	int playid = socket_getint(sockfd);
	printf("You are player %d (Total %d players)\n", playid + 1, play);

	while(1){
		int nxt_player = socket_getint(sockfd);
		if(nxt_player == -10000) break;
		int nxt_hand = socket_getint(sockfd);

		int my_nhands = -1;

		printf("\nCurrent Table:\n");
		for(int i = 0; i < play; i++){
			int nhands = socket_getint(sockfd);
			if(i == playid) my_nhands = nhands;
			printf("  Player %d:  ", i+1);

			for(int j = 0; j < nhands; j++){
				int doublebet = socket_getint(sockfd);
				int handsz = socket_getint(sockfd);
				vector<int> hand;

				if(doublebet) printf("<D> ");

				for(int k = 0; k < handsz; k++){
					int poker = socket_getint(sockfd);
					hand.push_back(poker);

					if(k != 0) printf(" ");
					if(poker / 4 + 1 >= 2 && poker / 4 + 1 <= 10)
						printf("%d%c", poker / 4 + 1, suit[poker % 4]);
					else
						printf("%c%c", face[poker / 4], suit[poker % 4]);
				}

				printf(" = %d", calc_value(hand));
				if(i == nxt_player && j == nxt_hand)
					printf("*");
				printf(" / ");
			}
			printf("\n");
		}

		if(my_nhands == -1)
			fprintf(stderr, "Impossible\n");

		printf("  Dealer  :  XX ");
		socket_getint(sockfd); // dealer's # of hand = 1
		socket_getint(sockfd); // double bet of dealer = 0
		socket_getint(sockfd); // dealer's # of cards = 2
		socket_getint(sockfd); // hole card
		int Dpoker = socket_getint(sockfd);
		if(Dpoker / 4 + 1 >= 2 && Dpoker / 4 + 1 <= 10)
			printf("%d%c / \n", Dpoker / 4 + 1, suit[Dpoker % 4]);
		else
			printf("%c%c / \n", face[Dpoker / 4], suit[Dpoker % 4]);

		int action_type = socket_getint(sockfd);

		if(nxt_player == playid){
			int nhand = socket_getint(sockfd);
			vector<int> hand;
			for(int i = 0; i < nhand; i++)
				hand.push_back(socket_getint(sockfd));

			printf("Your poker hand: ");
			for(auto poker : hand){
				if(poker / 4 + 1 >= 2 && poker / 4 + 1 <= 10)
					printf("%d%c", poker / 4 + 1, suit[poker % 4]);
				else
					printf("%c%c", face[poker / 4], suit[poker % 4]);
				printf(" ");
			}
			printf("= %d\n", calc_value(hand));

			if(action_type == 1){ // Side Rules
				if(nhand != 2) fprintf(stderr, "Impossible\n");

				printf("Your current money: %d, bet: %d\n", curMoney, bet);
				printf("Do you want to apply side rules? ");
				printf("[N]o side rule");
				if(bet <= curMoney)
					printf(" or [D]ouble down");
				if(bet <= curMoney && my_nhands < 4 && poker_value(hand[0]) == poker_value(hand[1]))
					printf(" or [S]plit");
				printf(": ");

				char siderule[100];
				fgets(siderule, 100, stdin);

				while( 
					 !(
						siderule[0] == 'N'
					 || (bet <= curMoney && siderule[0] == 'D')
					 || (bet <= curMoney && my_nhands < 4
						 && poker_value(hand[0]) == poker_value(hand[1])
						 && siderule[0] == 'S')
					  )
					 ){
					printf("Wrong input, Re-enter: ");
					fgets(siderule, 100, stdin);
				}

				if(siderule[0] == 'N') socket_sendint(sockfd, -1);
				if(siderule[0] == 'S') socket_sendint(sockfd, 0), curMoney -= bet;
				if(siderule[0] == 'D') socket_sendint(sockfd, 1), curMoney -= bet;
			}

			if(action_type == 2){ // Hit or Stand
				printf("[H]it or [S]tand? ");
				char HoS[100];
				fgets(HoS, 100, stdin);

				while(!(HoS[0] == 'H' || HoS[0] == 'S')){
					printf("Wrong input, Re-enter: ");
					fgets(HoS, 100, stdin);
				}

				if(HoS[0] == 'H') socket_sendint(sockfd, 1);
				if(HoS[0] == 'S') socket_sendint(sockfd, 0);
			}
		}

		int action = socket_getint(sockfd);

		if(action_type == 1){
			if(action == -1){
				if(nxt_player != playid)
					printf("Player %d did not use any side rules\n", nxt_player+1);
				else
					printf("You did not use any side rules\n");
			}
			if(action == 0){
				if(nxt_player != playid)
					printf("Player %d apply Split\n", nxt_player+1);
				else
					printf("You apply Split\n");
			}
			if(action == 1){
				if(nxt_player != playid)
					printf("Player %d Double Down\n", nxt_player+1);
				else
					printf("You Double Down\n");
			}
			if(action == 2){
				if(nxt_player != playid)
					printf("Player %d Double Down and Busted!\n", nxt_player+1);
				else
					printf("You Double Down and Busted!\n");
			}
		}

		if(action_type == 2){
			if(action == 0){
				if(nxt_player != playid)
					printf("Player %d stand\n", nxt_player+1);
				else
					printf("You stand\n");
			}
			if(action == 1){
				if(nxt_player != playid)
					printf("Player %d hit\n", nxt_player+1);
				else
					printf("You hit\n");
			}
			if(action == 2){
				if(nxt_player != playid)
					printf("Player %d hit and Busted!\n", nxt_player+1);
				else
					printf("You hit and Busted!\n");
			}
		}

		sleep(1);
	}

	printf("\nCurrent Table:\n");
	for(int i = 0; i < play + 1; i++){
		int nhands = socket_getint(sockfd);
		if(i == play) printf("  Dealer  :  ");
		else printf("  Player %d:  ", i+1);

		for(int j = 0; j < nhands; j++){
			int doublebet = socket_getint(sockfd);
			int handsz = socket_getint(sockfd);
			vector<int> hand;

			if(doublebet) printf("<D> ");

			for(int k = 0; k < handsz; k++){
				int poker = socket_getint(sockfd);
				hand.push_back(poker);
				if(k != 0) printf(" ");
				if(poker / 4 + 1 >= 2 && poker / 4 + 1 <= 10)
					printf("%d%c", poker / 4 + 1, suit[poker % 4]);
				else
					printf("%c%c", face[poker / 4], suit[poker % 4]);
			}

			printf(" = %d", calc_value(hand));
			if(i == play && j == 0)
				printf("*");
			printf(" / ");
		}
		printf("\n");
	}
	printf("Dealer keep hitting until the hand can >= 17, then stop\n");
	sleep(1);

	printf("\nFinal Table:\n");
	for(int i = 0; i < play + 1; i++){
		int nhands = socket_getint(sockfd);
		if(i == play) printf("  Dealer  :  ");
		else printf("  Player %d:  ", i+1);

		for(int j = 0; j < nhands; j++){
			int doublebet = socket_getint(sockfd);
			int handsz = socket_getint(sockfd);
			vector<int> hand;

			if(doublebet) printf("<D> ");

			for(int k = 0; k < handsz; k++){
				int poker = socket_getint(sockfd);
				hand.push_back(poker);
				if(k != 0) printf(" ");
				if(poker / 4 + 1 >= 2 && poker / 4 + 1 <= 10)
					printf("%d%c", poker / 4 + 1, suit[poker % 4]);
				else
					printf("%c%c", face[poker / 4], suit[poker % 4]);
			}

			printf(" = %d", calc_value(hand));
			printf(" / ");
		}
		printf("\n");
	}

	printf("\nEvaluating win & lost...\n");
	sleep(1);
	
	for(int i = 0; i < play; i++){
		int nhand = socket_getint(sockfd);
		printf("Player %d: ", i+1);
		for(int j = 0; j < nhand; j++){
			int type = socket_getint(sockfd);
			if(type == -2) printf("BUST :(");
			if(type == -1) printf("lost :(");
			if(type == 0) printf("push ._.");
			if(type == 1) printf("win :)");
			if(type == 50) printf("Dragon! 3x");
			if(type == 100) printf("BlackJack! 1.5x");
			if(type == 1000) printf("Royal BlackJack! 5x");
			printf(" / ");
		}
		printf("\n");
	}
	printf("\n");

	int earn = socket_getint(sockfd);
	if(earn >= 0) printf("Congratulation! You earn %d dollars\n", earn);
	else printf("Sorry, you lost %d dollars\n", -earn);

	curMoney = socket_getint(sockfd);
	printf("You now have %d dollar\n", curMoney);

	printf("\nGoing back to Game Menu ...\n");
	sleep(1);
}

void GameMenu(){
	curMoney = socket_getint(sockfd);
	printf("You currently have $%d\n", curMoney);

	while(1){
		int gametype = -7;
		printf("\nWhich game do you want to play?\n");
		printf("Press 1 for Guess Number, 2 for Black Jack, -1 to Quit: ");
		while(gametype != 1 && gametype != 2 && gametype != -1){
			char buff[100];
			fgets(buff, 100, stdin);
			int ret = sscanf(buff, "%d", &gametype);
			if(ret != 1) gametype = -7;

			if(gametype != 1 && gametype != 2 && gametype != -1)
				printf("No such option or wrong input. Please re-enter: ");
		}
		if(gametype == -1)
			printf("Bye Bye ~\n");
		if(gametype == 1){
			printf("\nGoing to gaming room of Guess Number ...\n");
			sleep(1);
		}
		if(gametype == 2){
			printf("\nGoing to gaming room of Black Jack ...\n");
			sleep(1);
		}

		socket_sendint(sockfd, gametype);
		if(gametype == -1) return;

		play = -7, room = -7;

		while(1){
			for(int i = 1; i <= 4; i++){
				printf("%d-Player Rooms: \n", i);
				for(int j = 0; j < 3; j++){
					int num_player = socket_getint(sockfd);
					printf("    Room %c has %d people\n", 'A'+j, num_player);
				}
			}

			printf("Please enter your desired \"#_OF_PLAYER<space>(A, B or C)\"\n");
			printf("(Any invalid player number or room id, goes back to game selection): ");

			char buff[100];
			fgets(buff, 100, stdin);
			char roomchar[10];
			int ret = sscanf(buff, "%d%s", &play, roomchar);

			if(ret != 2 || strlen(roomchar) != 1
			|| !(roomchar[0] >= 'A' && roomchar[0] <= 'Z')){
				play = -7;
				room = -7;
			}
			else
				room = roomchar[0] - 'A';

			socket_sendint(sockfd, play);
			socket_sendint(sockfd, room);

			if(!(play >= 1 && play <= 4 && room >= 0 && room <= 2))
				break;

			int accepted = socket_getint(sockfd);
			if(accepted == 1){
				if(gametype == 1) StartGame1();
				else StartGame2();

				if(curMoney < 10){
					printf("I have sucked all your money!\n");
					printf("You are kicked out of momo casino!\n");
					return;
				}

				break;
			}
			else{
				printf("\nSorry...Selected Game Room is currently full...\n");
				sleep(1);
			}
		}
	}
}

int main(int argv, char* argc[]){
	if(argv != 3){
		fprintf(stderr, "Usage: ./client hostname port\n");
		exit(-1);
	}

	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in serverAddress;
	bzero(&serverAddress, sizeof(serverAddress));

	serverAddress.sin_family = AF_INET;
	struct hostent *hp = gethostbyname(argc[1]);
	bcopy(hp->h_addr, &(serverAddress.sin_addr.s_addr), hp->h_length);
	//serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
	serverAddress.sin_port = htons(atoi(argc[2]));

	if(connect(sockfd, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0){
		printf("Server Not Turned On\n");
		return 0;
	}

	printf("!! The system has some pauses to assist human reading !!\n");
	sleep(1);
	
	GameMenu();
}
