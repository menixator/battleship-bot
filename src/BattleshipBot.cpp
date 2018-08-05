// BattleshipBot.cpp : Defines the entry point for the console application.
//

#include <math.h>
#include <iostream>

#include <arpa/inet.h>  //inet_addr
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>  //write

#pragma comment(lib, "wsock32.lib")

#define SHIPTYPE_BATTLESHIP "0"
#define SHIPTYPE_FRIGATE "1"
#define SHIPTYPE_SUBMARINE "2"

#define STUDENT_NUMBER "S1700804"
#define STUDENT_FIRSTNAME "Ahmed"
#define STUDENT_FAMILYNAME "Miljau"
#define MY_SHIP SHIPTYPE_BATTLESHIP

#define IP_ADDRESS_SERVER "127.0.0.1"
//#define IP_ADDRESS_SERVER "164.11.80.69"

#define PORT_SEND 1924     // We define a port that we are going to use.
#define PORT_RECEIVE 1925  // We define a port that we are going to use.

#define MAX_BUFFER_SIZE 500
#define MAX_SHIPS 200

#define MOVE_LEFT -1
#define MOVE_RIGHT 1
#define MOVE_UP 1
#define MOVE_DOWN -1
#define MOVE_FAST 2
#define MOVE_SLOW 1

struct sockaddr_in sendto_addr;
struct sockaddr_in receive_addr;

int sock_send;  // This is our socket, it is the handle to the IO address to
                // read/write packets
int sock_recv;  // This is our socket, it is the handle to the IO address to
                // read/write packets

char InputBuffer[MAX_BUFFER_SIZE];

int myX;
int myY;
int myHealth;
int myFlag;
int myType;

int number_of_ships;
int shipX[MAX_SHIPS];
int shipY[MAX_SHIPS];
int shipHealth[MAX_SHIPS];
int shipFlag[MAX_SHIPS];
int shipType[MAX_SHIPS];

bool message = false;
char MsgBuffer[MAX_BUFFER_SIZE];

bool fire = false;
int fireX;
int fireY;

bool moveShip = false;
int moveX;
int moveY;

bool setFlag = true;
int new_flag = 0;

void send_message(char *dest, char *source, char *msg);
void fire_at_ship(int X, int Y);
void move_in_direction(int left_right, int up_down);
void set_new_flag(int newFlag);

/*************************************************************/
/********* Your tactics code starts here *********************/
/*************************************************************/

#define MY_FLAG 10;
#define FIRING_RANGE 100
#define PARANOID 1
#define VISIBLE_RANGE 200

int up_down = MOVE_DOWN * MOVE_SLOW;
int left_right = MOVE_RIGHT * MOVE_SLOW;

int nFriends;
int nEnemies;

struct Ship {
  int id;
  int x;
  int y;
  int health;
  int flag;
  int distance;
  int type;
};
Ship allShips[MAX_SHIPS];
Ship *me;

Ship *enemies[MAX_SHIPS];
Ship *friends[MAX_SHIPS];

bool isFriendly(int index) {
#if PARANOID
  return false;
#else
  return allShips[index].flag == MY_FLAG;
#endif
}

void printShip(Ship *ship) {
  printf("Ship{x=%d, y=%d, health=%d, flag=%d, type=%d}\n", ship->x, ship->y,
         ship->health, ship->flag, ship->type);
}

void tactics() {
  int i;
  set_new_flag(10);

  nFriends = 0;
  nEnemies = 0;

  memset(enemies, 0, sizeof enemies);
  memset(friends, 0, sizeof friends);
  memset(allShips, 0, sizeof allShips);

  allShips[0].id = i;
  allShips[0].x = myX;
  allShips[0].y = myY;
  allShips[0].flag = myFlag;
  allShips[0].distance = 0;
  allShips[0].type = myType;
  allShips[0].health = myHealth;
  me = &allShips[0];

  if (number_of_ships > 1) {
    for (i = 1; i < number_of_ships; i++) {
      // Initialize struct
      allShips[i].id = i;
      allShips[i].x = shipX[i];
      allShips[i].y = shipY[i];
      allShips[i].flag = shipFlag[i];
      allShips[i].distance =
          (int)sqrt((double)((shipX[i] - shipX[0]) * (shipX[i] - shipX[0]) +
                             (shipY[i] - shipY[0]) * (shipY[i] - shipY[0])));
      allShips[i].type = shipType[i];
      allShips[i].health = shipHealth[i];

      if (isFriendly(i)) {
        printShip(&allShips[i]);
        printf("friend with flag: %d\n", shipFlag[i]);

        nFriends++;
        friends[nFriends] = &allShips[i];
      } else {
        enemies[nEnemies] = &allShips[i];
        nEnemies++;
      }
    }

    if (nEnemies > 0) {
      int target = -1;

      for (int i = 0; i < nEnemies; i++) {
        if (target == -1 || (
                // If the ship is withing firing range
                enemies[i]->distance <= FIRING_RANGE &&
                // and it's closer
                ((enemies[target]->distance > enemies[i]->distance
                  // or it has lower health
                  || enemies[target]->health > enemies[i]->health)))) {
          target = i;
        }
      }
      // Do we have an ideal ship to fire at?
      if (target > -1) {
        printf(
            "firing at ship at (%d, %d) with health (%d) and distance (%d)\n",
            enemies[target]->x, enemies[target]->y, enemies[target]->health,
            enemies[target]->distance);
        fire_at_ship(enemies[target]->x, enemies[target]->y);
      }
    }
  } else {
    printf("no enemies in sight\n");
  }

  // printf("current coordinates: (%d, %d)\n", myX, myY);
  if (myY > 900) {
    up_down = MOVE_DOWN * MOVE_FAST;
  }

  if (myX < 200) {
    left_right = MOVE_RIGHT * MOVE_FAST;
  }

  if (myY < 100) {
    up_down = MOVE_UP * MOVE_FAST;
  }

  if (myX > 800) {
    left_right = MOVE_LEFT * MOVE_FAST;
  }
  move_in_direction(left_right, up_down);
}

void messageReceived(char *msg) { printf("message recieved: %s\n", msg); }

/*************************************************************/
/********* Your tactics code ends here ***********************/
/*************************************************************/

void communicate_with_server() {
  char buffer[4096];
  int len = sizeof(sockaddr);
  char chr;
  bool finished;
  int i;
  int j;
  int rc;
  char *p;

  sprintf(buffer, "Register  %s,%s,%s,%s", STUDENT_NUMBER, STUDENT_FIRSTNAME,
          STUDENT_FAMILYNAME, MY_SHIP);
  sendto(sock_send, buffer, strlen(buffer), 0, (sockaddr *)&sendto_addr,
         sizeof(sockaddr));
  while (true) {
    if (recvfrom(sock_recv, buffer, sizeof(buffer) - 1, 0,
                 (sockaddr *)&receive_addr, (socklen_t *)&len)) {
      p = ::inet_ntoa(receive_addr.sin_addr);

      if ((strcmp(IP_ADDRESS_SERVER, "127.0.0.1") == 0) ||
          (strcmp(IP_ADDRESS_SERVER, p) == 0)) {
        if (buffer[0] == 'M') {
          messageReceived(buffer);
        } else {
          i = 0;
          j = 0;
          finished = false;
          number_of_ships = 0;

          while ((!finished) && (i < 4096)) {
            chr = buffer[i];

            switch (chr) {
              case '|':
                InputBuffer[j] = '\0';
                j = 0;
                sscanf(InputBuffer, "%d,%d,%d,%d,%d", &shipX[number_of_ships],
                       &shipY[number_of_ships], &shipHealth[number_of_ships],
                       &shipFlag[number_of_ships], &shipType[number_of_ships]);
                number_of_ships++;
                break;

              case '\0':
                InputBuffer[j] = '\0';
                sscanf(InputBuffer, "%d,%d,%d,%d,%d", &shipX[number_of_ships],
                       &shipY[number_of_ships], &shipHealth[number_of_ships],
                       &shipFlag[number_of_ships], &shipType[number_of_ships]);
                number_of_ships++;
                finished = true;
                break;

              default:
                InputBuffer[j] = chr;
                j++;
                break;
            }
            i++;
          }

          myX = shipX[0];
          myY = shipY[0];
          myHealth = shipHealth[0];
          myFlag = shipFlag[0];
          myType = shipType[0];
        }

        tactics();
        if (message) {
          sendto(sock_send, MsgBuffer, strlen(MsgBuffer), 0,
                 (sockaddr *)&sendto_addr, sizeof(sockaddr));
          message = false;
        }

        if (fire) {
          sprintf(buffer, "Fire %s,%d,%d", STUDENT_NUMBER, fireX, fireY);
          sendto(sock_send, buffer, strlen(buffer), 0, (sockaddr *)&sendto_addr,
                 sizeof(sockaddr));
          fire = false;
        }

        if (moveShip) {
          sprintf(buffer, "Move %s,%d,%d", STUDENT_NUMBER, moveX, moveY);
          rc = sendto(sock_send, buffer, strlen(buffer), 0,
                      (sockaddr *)&sendto_addr, sizeof(sockaddr));
          moveShip = false;
        }

        if (setFlag) {
          sprintf(buffer, "Flag %s,%d", STUDENT_NUMBER, new_flag);
          sendto(sock_send, buffer, strlen(buffer), 0, (sockaddr *)&sendto_addr,
                 sizeof(sockaddr));
          setFlag = false;
        }
      }
    } else {
    }
  }

  printf("Student %s\n", STUDENT_NUMBER);
}

void send_message(char *dest, char *source, char *msg) {
  message = true;
  sprintf(MsgBuffer, "Message %s,%s,%s,%s", STUDENT_NUMBER, dest, source, msg);
}

void fire_at_ship(int X, int Y) {
  fire = true;
  fireX = X;
  fireY = Y;
}

void move_in_direction(int X, int Y) {
  if (X < -2) X = -2;
  if (X > 2) X = 2;
  if (Y < -2) Y = -2;
  if (Y > 2) Y = 2;

  moveShip = true;
  moveX = X;
  moveY = Y;
}

void set_new_flag(int newFlag) {
  setFlag = true;
  new_flag = newFlag;
}

int main(int argc, char *argv[]) {
  char chr = '\0';

  printf("\n");
  printf("Battleship Bots\n");
  printf("UWE Computer and Network Systems Assignment 2 (2016-17)\n");
  printf("\n");

  sock_send = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);  // Here we create our
                                                         // socket, which will
                                                         // be a UDP socket
                                                         // (SOCK_DGRAM).
  if (!sock_send) {
    printf("Socket creation failed!\n");
  }

  sock_recv = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);  // Here we create our
                                                         // socket, which will
                                                         // be a UDP socket
                                                         // (SOCK_DGRAM).
  if (!sock_recv) {
    printf("Socket creation failed!\n");
  }

  sendto_addr.sin_family = AF_INET;
  sendto_addr.sin_addr.s_addr = inet_addr(IP_ADDRESS_SERVER);
  sendto_addr.sin_port = htons(PORT_SEND);

  receive_addr.sin_family = AF_INET;
  //	receive_addr.sin_addr.s_addr = inet_addr(IP_ADDRESS_SERVER);
  receive_addr.sin_addr.s_addr = INADDR_ANY;
  receive_addr.sin_port = htons(PORT_RECEIVE);

  int ret = bind(sock_recv, (sockaddr *)&receive_addr, sizeof(sockaddr));
  //	int ret = bind(sock_send, (sockaddr *)&receive_addr, sizeof(sockaddr));
  if (ret) {
  }

  communicate_with_server();

  close(sock_send);
  close(sock_recv);

  while (chr != '\n') {
    chr = getchar();
  }

  return 0;
}
