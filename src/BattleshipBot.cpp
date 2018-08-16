/***************************************************
 * BattleShipBot.cpp
 * Author:      Ahmed Miljau
 * Student Id:  S1700804
 * Defines the entrypoint of the application
 *
 ***************************************************
 *                  CHANGES
 ***************************************************
 * v0.1: Improved tactics to follow and kill
 *  weakest and closest enemy
 * v0.2: Add rating system
 */

#include <iostream>
#include <math.h>

#include <arpa/inet.h> //inet_addr
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h> //write

#define SHIPTYPE_BATTLESHIP "0"
#define SHIPTYPE_FRIGATE "1"
#define SHIPTYPE_SUBMARINE "2"

#define STUDENT_NUMBER student_id
#define STUDENT_FIRSTNAME "Ahmed"
#define STUDENT_FAMILYNAME "Miljau"
#define MY_SHIP SHIPTYPE_BATTLESHIP

//#define IP_ADDRESS_SERVER "127.0.0.1"
#define IP_ADDRESS_SERVER server_ip

#define PORT_SEND 1924    // Port to send data to
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

// Whether or not we are running away
bool isRunningAway = false;
int runAwayStart = -1;

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

void send_message(char *dest, char *source, char *msg);
void fire_at_ship(int X, int Y);
void move_in_direction(int left_right, int up_down);
void set_new_flag(int newFlag);

/*************************************************************/
/********* Your tactics code starts here *********************/
/*************************************************************/

// Our Flag
#define MY_FLAG 67678

// Define the firing range for ease of use
#define FIRING_RANGE 100

// Visible Range for convenience
#define VISIBLE_RANGE 200

// Maximum of the tick variable
#define TICK_MAX 4294967295

// A Test mode
#define PARANOID 0

// A macro used to turn debugging on and off
#define DEBUG 0


int new_flag = MY_FLAG;
// Some macros for convenience defining the grid edges
#define MIN_X 5
#define MIN_Y 5
#define MAX_X 995
#define MAX_Y 995

// Debugging setup
#if DEBUG
#define DEBUG_FILE "debug.log"
FILE *debug_fd;
#define debug(...)                                                             \
  do {                                                                         \
    fprintf(debug_fd, __VA_ARGS__);                                            \
    printf(__VA_ARGS__);                                                       \
    fflush(debug_fd);                                                          \
  } while (0)
#else
#define debug(...)                                                             \
  do {                                                                         \
  } while (0)
#endif

// An Enumerator that defines the speed
enum SPEED { SLOW, REST, FAST };

// An enumerator the defines the direction on an axis
enum BEARING { POS, NEG, ZERO };


// Defines the direction
struct Bearings {
  BEARING hBearing;
  BEARING vBearing;
  SPEED hSpeed;
  SPEED vSpeed;
};

// Used to keep track of the state and add different
// distinctions to behavior using the number of ticks passed
unsigned int ticks = 0;


// Number of friends
int nFriends;
// Number of enemies
int nEnemies;


// Coordinates structure. Defines an (x, y)
struct Coordinates {
  int x;
  int y;
};

// A Ship structure that groups all the information about the
// ship
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

// allShips will contain all the visible ships
Ship allShips[MAX_SHIPS];

// Me is a pointer to our own ship
Ship *me;

// enemies is a pointer array to all the enemies that are
// visible
Ship *enemies[MAX_SHIPS];
// friends is a pointer array to all the friends
Ship *friends[MAX_SHIPS];

// Stores my ally's information
Ship alsan;
bool messageRecievedFromAlsan = false;

// Returns if the ship at index is a friendly
bool isFriendly(int index) {
// Paranoid mode for testing.
#if PARANOID
  return false;
#else
  return allShips[index].flag == new_flag;
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

// Last bearing that the ship was moving.
NAMED_BEARING last_bearings = STATIONARY;


// The movements that the ship is allowed to make.
NAMED_BEARING bearings[] = {NORTH,      SOUTH,      EAST,       WEST,
                            NORTH_EAST, NORTH_WEST, SOUTH_EAST, SOUTH_WEST};


// Fires at ta coordinate
void fireAt(Coordinates coords) { fire_at_ship(coords.x, coords.y); }

// Dumps the ship information to stdout
void printShip(Ship *ship) {
  debug("Ship{id=%d, x=%d, y=%d, health=%d, flag=%d, type=%d, distance=%d}\n",
        ship->id, ship->coords.x, ship->coords.y, ship->health, ship->flag,
        ship->type, ship->distance);
}

// Fires at a ship
void fireAtShip(Ship *ship) {
  debug("Firing at: ");
  printShip(ship);
  fire_at_ship(ship->coords.x, ship->coords.y);
}

// Dumps Bearing strctures. Used for debugging.
void printBearings(Bearings bearings) {
  debug("Bearings{horizontal=%d, vertical=%d, hSpeed=%d, vSpeed=%d}\n",
        bearings.hBearing, bearings.vBearing, bearings.hSpeed, bearings.vSpeed);
}

// Converts a named bearing such as NORTH or SOUTH to a Bearing struct
int namedBearingToBearings(Bearings *bearing, NAMED_BEARING namedBearing,
                           SPEED speed) {
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

// Move with bearings
void move(Bearings bearings) {
  printBearings(bearings);
  move_in_direction(bearings.hBearing * bearings.hSpeed,
                    bearings.vBearing * bearings.vSpeed);
}
// Gives the Euclidean distnace between two coordinates.
int diff(Coordinates a, Coordinates b) {
  int ret = (int)sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
  if (ret < 0) {
    printf("diff: (%d, %d), (%d, %d)%d\n", a.x, a.y, b.x, b.y, ret);
  }
  assert(ret >= 0);
  return ret;
}

// Gets the new coordinate if the ship moved in the direction specified
// by the named bearing. eg: (0,0) + NORTH = (0,1)
int getNewCoordinate(Coordinates *coords, NAMED_BEARING namedBearing,
                     SPEED speed) {
  Bearings bearings;
  namedBearingToBearings(&bearings, namedBearing, speed);

  coords->x = me->coords.x + bearings.hBearing * bearings.hSpeed;
  coords->y = me->coords.y + bearings.vBearing * bearings.vSpeed;
  return 0;
}

// Returns whether or not the coordinates are close to the coordinates.
bool isCloseToEdge(Coordinates coords) {
  return coords.x <= 40 || coords.x >= 960 || coords.y <= 40 || coords.y >= 960;
}

// Returns whether or not the coordinate is out of the grid.
bool isOutOfGrid(Coordinates coords) {
  return coords.x <= MIN_X || coords.x >= MAX_X || coords.y <= MIN_Y ||
         coords.y >= MAX_Y;
}

// Returns whether or not coordinates a and b share a x or y coordinate.
bool isAligned(Coordinates a, Coordinates b) {
  return a.x == b.x  || a.y == b.y;
}

// Rates a coordinate based on various factors. The bigger the rating is,
// the better it is.
int rate_coordinate(NAMED_BEARING bearing, Coordinates coords, Ship *ship) {
  // Rating starts at 0
  int rating = 0;

  for (int i = 0; i < nEnemies; i++) {
    int pDistance = diff(me->coords, enemies[i]->coords);
    int distance = diff(coords, enemies[i]->coords);

    if (ship != NULL && enemies[i]->id == ship->id) {
      if (diff(coords, ship->coords) < diff(me->coords, ship->coords)) {
        rating += 4096 * ((VISIBLE_RANGE - distance) + enemies[i]->health);
      }
      continue;
    }

    rating -= 4096 * ((VISIBLE_RANGE - distance) + enemies[i]->health);

    if (distance < pDistance) {
      rating -= 14000;
    } else if (distance > pDistance) {
      rating += 14000;
    }

    if (isAligned(enemies[i]->coords, coords)) {
      rating -= 10000;
    }
  }

  // rating -= 1000*(abs(500 - coords.x) + abs(500-coords.y));

  if (last_bearings != STATIONARY && last_bearings == bearing) {
    rating += 7500;
    if (isRunningAway && ticks - runAwayStart > 0) {
      rating += 25000;
    }
  }

  if (messageRecievedFromAlsan && alsan.distance > VISIBLE_RANGE) {
    if (ship == NULL) {
      if (diff(coords, alsan.coords) < alsan.distance) {
        rating += 100000;
      }
    } else {
      if (diff(coords, alsan.coords) < alsan.distance) {
        rating += 50000;
      }
    }
  }

  Coordinates center;
  center.x = 500;
  center.y = 500;

  rating -= 1000 * diff(coords, center);

  return rating;
}

// Compares directions a and b. if a is better, a negative value is returned,
// if b is better, a positive value is returned. If they're both equal,
// 0 is returned.
int cmp_direction(const void *a, const void *b, void *p_ship) {
  NAMED_BEARING bearingA = *(NAMED_BEARING *)a;
  NAMED_BEARING bearingB = *(NAMED_BEARING *)b;
  Ship *ship = (Ship *)p_ship;
  Coordinates coordA, coordB;
  getNewCoordinate(&coordA, bearingA, FAST);
  getNewCoordinate(&coordB, bearingB, FAST);

  if (isOutOfGrid(coordA) && !isOutOfGrid(coordB)) {
    return -1;
  } else if (isOutOfGrid(coordB) && !isOutOfGrid(coordA)) {
    return 1;
  } else if (isOutOfGrid(coordA) && isOutOfGrid(coordB)) {
    return 0;
  }
  int bRating = rate_coordinate(bearingB, coordB, ship);
  int aRating = rate_coordinate(bearingA, coordA, ship);
  if (bRating - aRating == 0) {
    return ticks > 500 ? 1 : -1;
  }
  return bRating - aRating;
}

// Moves toward a ship. Ship can be null.
void moveTowards(Ship *ship) {
  if (ship != NULL) {
    debug("\tMoving Towards: ");
    printShip(ship);
  }
  qsort_r(bearings, sizeof(bearings) / sizeof(*bearings), sizeof(bearings[0]),
          cmp_direction, (void *)ship);

  Bearings new_bearings;
  last_bearings = bearings[0];

  namedBearingToBearings(&new_bearings, *bearings, FAST);
  move(new_bearings);
}

// Rates a ship based on various factors.
// A bigger value means it is worse. 
int rate_ship(Ship *ship) {
  int rating = 0;
  int distance;

  for (int i = 1; i < number_of_ships; i++) {
    if (allShips[i].id == ship->id || isFriendly(i))
      continue;
    distance = diff(ship->coords, allShips[i].coords);
    if (distance < FIRING_RANGE) {
      rating += 100;
    }
  }

  rating += ship->health - me->health;
  rating += ship->distance;
  return rating;
}

// Compares two ships.
int cmp_ship(const void *a, const void *b) {
  Ship **shipA = (Ship **)a;
  Ship **shipB = (Ship **)b;
  return rate_ship(*shipA) - rate_ship(*shipB);
}

// Start of the tactics method.
void tactics() {
  if (ticks >= TICK_MAX) {
    ticks %= TICK_MAX;
  }
  ticks++;

  debug("tick: %d\n", ticks);

  int i;

  // target to fire at.
  Ship *target = NULL;

  nFriends = 0;
  nEnemies = 0;

  // memset(allShips, 0, sizeof(allShips)*sizeof(Ship));
  // memset(enemies, 0, sizeof(Ship*)*sizeof(enemies));
  // memset(friends, 0, sizeof(Ship*)*sizeof(friends));

  allShips->id = 0;
  allShips->coords.x = myX;
  allShips->coords.y = myY;
  allShips->flag = myFlag;
  allShips->distance = 0;
  allShips->type = myType;
  allShips->health = myHealth;

  me = allShips;
  debug("\t Me: ");
  printShip(me);

  if (number_of_ships > 1) {
    debug("more than one ship visible\n");
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
        debug("\t ALLY: ");
        printShip(&allShips[i]);
        friends[nFriends] = &allShips[i];
        nFriends++;
      } else {
        debug("\t ENEMY: ");
        printShip(&allShips[i]);
        enemies[nEnemies] = &allShips[i];
        nEnemies++;
      }
    }
    debug("\n");

  } else {
    debug("no ships visible\n");
  }

  if (nEnemies > 0) {
    qsort(enemies, nEnemies, sizeof(enemies[0]), cmp_ship);
    if (rate_ship(enemies[0]) < 10 + 150 + 200) {
      isRunningAway = false;
      target = *enemies;
    } else {
      if (enemies[0]->distance <= FIRING_RANGE) {
        fireAtShip(enemies[0]);
      }
      runAwayStart = ticks;
      isRunningAway = true;
    }
  } else {
    if (isRunningAway && ticks - runAwayStart > 500) {
      isRunningAway = false;
      runAwayStart = -1;
    }
  }

  // Do we have an ideal ship to fire at?
  if (target != NULL) {
    debug("\t Target Selected: ");
    printShip(target);
    moveTowards(target);

    if (target->distance <= FIRING_RANGE) {
      debug("\tFiring at Enemy: ");
      printShip(target);
      fireAtShip(target);
    }
  } else {
    moveTowards(target);
    if (nFriends > 0 && abs(me->health - friends[0]->health) >= 5) {
      fireAtShip(&alsan);
    }
  }
  char msg[100];
  sprintf(msg, "%d %d", me->coords.x, me->coords.y);
  send_message("S1800083", "S1700804", msg);
  debug("\n");
}

void messageReceived(char *msg) {
  int x, y;
  if (ticks == 0)
    return;
  debug("Message recieved! '%s'\n", msg);
  int ret = sscanf(msg, "Message S1700804, S1800083, %d %d", &x, &y);
  if (!ret)
    return;
  alsan.coords.x = x;
  alsan.coords.y = y;
  alsan.distance = diff(me->coords, alsan.coords);

  if (!messageRecievedFromAlsan)
    messageRecievedFromAlsan = true;
  debug("ASLAN is at (%d, %d)\n", x, y);
}

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
          sendto(sock_send, buffer, strlen(buffer), 0, (sockaddr *)&sendto_addr,
                 sizeof(sockaddr));
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
#if DEBUG
  debug_fd = fopen(DEBUG_FILE, "w+");
  if (debug_fd == NULL) {
    printf("ERROR: couldn't open debug file\n");
    return -1;
  }
#endif
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

  debug("The server ip is: %s\n", server_ip);
  debug("The client interface ip is: %s\n",
        bind_ip == NULL ? "0.0.0.0" : bind_ip);
  debug("The student id is: %s\n", student_id);

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
  receive_addr.sin_addr.s_addr =
      bind_ip == NULL ? INADDR_ANY : inet_addr(bind_ip);
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
