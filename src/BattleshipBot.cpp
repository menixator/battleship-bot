// BattleshipBot.cpp : Defines the entry point for the console application.
//

#include <iostream>
#include <math.h>

#include <arpa/inet.h> //inet_addr
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h> //write
#include <assert.h>

#pragma comment(lib, "wsock32.lib")


#define SHIPTYPE_BATTLESHIP "0"
#define SHIPTYPE_FRIGATE "1"
#define SHIPTYPE_SUBMARINE "2"

#define STUDENT_NUMBER student_id
#define STUDENT_FIRSTNAME "Ahmed"
#define STUDENT_FAMILYNAME "Miljau"
#define MY_SHIP SHIPTYPE_BATTLESHIP

//#define IP_ADDRESS_SERVER "127.0.0.1"
#define IP_ADDRESS_SERVER server_ip

#define PORT_SEND 1924   // Port to send data to
#define PORT_RECEIVE 1925 // Port to recieve data from

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

int sock_send; // This is our socket, it is the handle to the IO address to
               // read/write packets
int sock_recv; // This is our socket, it is the handle to the IO address to
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
char const *server_ip;
char const *default_server_ip = "127.0.0.1";
char const *student_id;

char const *bind_ip;

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
#define JIGGLE_DELTA 3
#define TICK_MAX 1024
#define CLAMP(target, min, max)                                                \
  ((target) <= min ? min : (target) >= max ? max : (target))
#define MAX_ALLIES 3

#define MIN_X 5;
#define MIN_Y 5;
#define MAX_X 995;
#define MAX_Y 995;

enum SPEED { SLOW, REST, FAST };
enum BEARING { POS, NEG, ZERO };

struct Bearings {
  BEARING hBearing;
  BEARING vBearing;
  SPEED hSpeed;
  SPEED vSpeed;
};

unsigned int ticks = 0;


int nFriends;
int nEnemies;

struct Coordinates {
  int x;
  int y;
};

struct Ship {
  int id;
  Coordinates coords;
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

// Returns if the ship at index is a friendly
bool isFriendly(int index) {
// Paranoid mode for testing.
#if PARANOID
  return false;
#else
  return allShips[index].flag == MY_FLAG;
#endif
}



// Setup simpler directions.
enum NAMED_BEARING {
  STATIONARY,
  NORTH,
  SOUTH,
  EAST,
  WEST,
  NORTH_EAST,
  NORTH_WEST,
  SOUTH_EAST,
  SOUTH_WEST
};

NAMED_BEARING last_bearings = STATIONARY;

NAMED_BEARING bearings[] = {
  NORTH,
  SOUTH,
  EAST,
  WEST,
  NORTH_EAST,
  NORTH_WEST,
  SOUTH_EAST,
  SOUTH_WEST
};

// Function definitions.
void fireAtShip(Ship *ship);
void printShip(Ship *ship);

void fireAt(Coordinates coords) { fire_at_ship(coords.x, coords.y); }
void fireAtShip(Ship *ship) {
    printf("Firing at: ");
    printShip(ship);
    fire_at_ship(ship->coords.x, ship->coords.y);
}

void printShip(Ship *ship) {
  printf("Ship{x=%d, y=%d, health=%d, flag=%d, type=%d, distance=%d}\n",
         ship->coords.x, ship->coords.y, ship->health, ship->flag, ship->type,
         ship->distance);
}

int namedBearingToBearings(Bearings *bearing, NAMED_BEARING namedBearing, SPEED speed){
  int vertical = 0;
  int horizontal = 0;
  switch (namedBearing) {
  case NORTH:
    vertical = MOVE_UP;
    break;

  case SOUTH:
    vertical = MOVE_DOWN;
    break;

  case EAST:
    horizontal = MOVE_RIGHT;
    break;

  case WEST:
    horizontal = MOVE_LEFT;
    break;

  case NORTH_EAST:
    vertical = MOVE_UP;
    horizontal = MOVE_RIGHT;
    break;

  case NORTH_WEST:
    vertical = MOVE_UP;
    horizontal = MOVE_LEFT;
    break;

  case SOUTH_EAST:
    vertical = MOVE_DOWN;
    horizontal = MOVE_RIGHT;
    break;

  case SOUTH_WEST:
    vertical = MOVE_DOWN;
    horizontal = MOVE_LEFT;
    break;

  case STATIONARY:
    break;
  }
  bearing->vBearing = (BEARING)vertical;
  bearing->hBearing = (BEARING)horizontal;

  bearing->vSpeed = (SPEED)speed;
  bearing->hSpeed = (SPEED)speed;
  return 0;
}


void move(Bearings bearings) {
    printf("Moving X: %d, Y: %d\n",bearings.hBearing*bearings.hSpeed, bearings.vBearing*bearings.vSpeed);
  move_in_direction(bearings.hBearing*bearings.hSpeed, bearings.vBearing*bearings.vSpeed);
}


int diff( Coordinates a, Coordinates b){
  int ret = (int) sqrt( (a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y) );
  assert(ret >= 0);
  return ret;
}

int getNewCoordinate(Coordinates *coords, NAMED_BEARING namedBearing, SPEED speed) {
  Bearings bearings;
  namedBearingToBearings(&bearings, namedBearing, speed);

  coords->x = me->coords.x + bearings.hBearing*bearings.hSpeed;
  coords->y = me->coords.y + bearings.vBearing*bearings.vSpeed;
  return 0;
}

#define HOVER_RANGE 10+((me->health/10)*50)

bool isCloseToEdge(Coordinates coords){
    return coords.x <= 10 || coords.x >= 980 || coords.y <= 10  || coords.y >= 980;
}

// isCoordAligned
bool isXorYSame(Coordinates a, Coordinates b){
    return abs(a.x-b.x) == 0 || abs(a.y-b.y) == 0 || (abs(a.x-b.x) == abs(a.y-b.y));
}

int isCoordDangerous(Coordinates coords, Ship *ship){
    int dangerousCount = 0;

    for(int i=0;i<nEnemies;i++){
        if (enemies[i]->id == ship->id) continue;
        if (diff(coords, enemies[i]->coords) <= 80){
            dangerousCount++;
        }
    }
    return dangerousCount;
}



int rate_coordinate(NAMED_BEARING bearing, Coordinates coords, Ship* ship){
    // Rating starts at 0
    int rating = 0;

    for(int i=0;i<nEnemies;i++){
        if (ship != NULL && enemies[i]->id == ship->id){

            if (diff(coords, ship->coords) < diff(me->coords, ship->coords)){
                rating-=500;
            }
            continue;
        }
        int pDistance = diff(me->coords, enemies[i]->coords);
        int distance = diff(coords, enemies[i]->coords);

        rating -= 4096*((VISIBLE_RANGE-distance)+enemies[i]->health);

        if (distance < pDistance){ rating-= 10000;} else if (distance > pDistance ){rating += 10000;}

        if ( isXorYSame(enemies[i]->coords, coords) ){
            rating -= 100;
        }
    }

    if (isCloseToEdge(coords)){
        rating -= 5000;
    }
    
    if (last_bearings != STATIONARY && last_bearings == bearing){
        rating += 2500;
    }
    rating += 5*(1000-abs(500-coords.x)-abs(500-coords.y));

    return rating;
}

int cmp_direction(const void *a, const void *b, void *p_ship){
  NAMED_BEARING bearingA = *(NAMED_BEARING*) a;
  NAMED_BEARING bearingB = *(NAMED_BEARING*) b;
  Ship *ship = (Ship*) p_ship;
  Coordinates coordA, coordB;
  getNewCoordinate(&coordA, bearingA, FAST);
  getNewCoordinate(&coordB, bearingB, FAST);
  int bRating = rate_coordinate(bearingB, coordB, ship);
  int aRating = rate_coordinate(bearingA, coordA, ship);

  // if a less mean big
  return bRating - aRating;
}




void moveTowards(Ship *ship) {
  if(ship != NULL){ 
    printShip(ship);
  }
  qsort_r(bearings, sizeof(bearings)/sizeof(*bearings), sizeof(bearings[0]), cmp_direction, ( void*)ship);

  Bearings new_bearings;
  last_bearings = bearings[0];

  namedBearingToBearings(&new_bearings, *bearings, FAST);
  move(new_bearings);
}




int cmp_ship(const void *a, const void *b) {
  Ship *shipA = (Ship *)a;
  Ship *shipB = (Ship *)b;

  if (shipA->health != shipB->health)
    return shipA->health - shipB->health;

  if (shipA->distance != shipB->distance)
    return shipA->distance - shipB->distance;
  return 0;
}

void tactics() {
  if (ticks >= TICK_MAX) {
    ticks %= TICK_MAX;
  } else {
    ticks++;
  }

  printf("tick: %d\n", ticks);

  int i;

  // target to fire at.
  Ship *target = NULL;


  nFriends = 0;
  nEnemies = 0;

  memset(enemies, 0, sizeof enemies);
  memset(friends, 0, sizeof friends);
  memset(allShips, 0, sizeof allShips);

  allShips->id = 0;
  allShips->coords.x = myX;
  allShips->coords.y = myY;
  allShips->flag = myFlag;
  allShips->distance = 0;
  allShips->type = myType;
  allShips->health = myHealth;

  me = allShips;

  
  if (number_of_ships > 1) {
    printf("more than one ship visible\n");
    for (i = 1; i < number_of_ships; i++) {
      // Initialize struct
      allShips[i].id = i;
      allShips[i].coords.x = shipX[i];
      allShips[i].coords.y = shipY[i];
      allShips[i].flag = shipFlag[i];
      allShips[i].distance = diff(me->coords, allShips[i].coords);
      allShips[i].type = shipType[i];
      allShips[i].health = shipHealth[i];

      if (isFriendly(i)) {
        printShip(&allShips[i]);

        nFriends++;
        friends[nFriends] = &allShips[i];
      } else {
        enemies[nEnemies] = &allShips[i];
        nEnemies++;
      }
    }

  } else {
    printf("no ships visible\n");
  }
  
  if (nEnemies > 0) {
    printf("sorting enemies\n");
    // Sort enemies.
    // nEnemies: sizeof array
    // sizeof(Ship*): enemies is a pointer array
    qsort(enemies, nEnemies, sizeof(Ship *), cmp_ship);
    target = *enemies;
  }

  // Do we have an ideal ship to fire at?
  if (target != NULL) {
    printf("target selected\n");
    printShip(target);
    moveTowards(target);

    if (target->distance <= FIRING_RANGE) {
      fireAtShip(target);
    }
  } else {
    printf("there wasn't any target\n");
    moveTowards(target);
  }

}

void messageReceived(char *msg) { printf("message recieved: '%s'\n", msg); }

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
  if (X < -2)
    X = -2;
  if (X > 2)
    X = 2;
  if (Y < -2)
    Y = -2;
  if (Y > 2)
    Y = 2;

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
  server_ip = getenv("SERVER_IP");

  if (server_ip == NULL) {
    server_ip = default_server_ip;
  }

  student_id = getenv("STUDENT_ID");

  if (student_id == NULL) {
    printf("ERROR: No student id provided\n");
    return -1;
  }

  bind_ip = getenv("BIND_IP");

  printf("The server ip is: %s\n", server_ip);
  printf("The client interface ip is: %s\n",
         bind_ip == NULL ? "0.0.0.0" : bind_ip);
  printf("The student id is: %s\n", student_id);

  printf("\n");
  printf("Battleship Bots\n");
  printf("UWE Computer and Network Systems Assignment 2 (2016-17)\n");
  printf("\n");

  sock_send = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP); // Here we create our
                                                        // socket, which will
                                                        // be a UDP socket
                                                        // (SOCK_DGRAM).
  if (!sock_send) {
    printf("Socket creation failed!\n");
    return -1;
  }

  sock_recv = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP); // Here we create our
                                                        // socket, which will
                                                        // be a UDP socket
                                                        // (SOCK_DGRAM).
  if (!sock_recv) {
    printf("Socket creation failed!\n");
    return -1;
  }

  sendto_addr.sin_family = AF_INET;
  sendto_addr.sin_addr.s_addr = inet_addr(IP_ADDRESS_SERVER);
  sendto_addr.sin_port = htons(PORT_SEND);

  receive_addr.sin_family = AF_INET;
  //	receive_addr.sin_addr.s_addr = inet_addr(IP_ADDRESS_SERVER);
  receive_addr.sin_addr.s_addr =INADDR_ANY;
  receive_addr.sin_port = htons(PORT_RECEIVE);

  int ret = bind(sock_recv, (sockaddr *)&receive_addr, sizeof(sockaddr));
  //	int ret = bind(sock_send, (sockaddr *)&receive_addr, sizeof(sockaddr));
  if (ret) {
    printf("Socket bind failed!");
    return -1;
  }

  communicate_with_server();

  close(sock_send);
  close(sock_recv);

  while (chr != '\n') {
    chr = getchar();
  }

  return 0;
}
