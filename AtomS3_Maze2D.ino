#include <M5AtomS3.h>
#include <stdlib.h>

const int MAZE_SIZE[] = { 15, 31, 63, 127 };
const int MAX_SIZE = 127;
unsigned char maze[MAX_SIZE * MAX_SIZE];
enum { WAY, GOAL, WALL };
const int DIR[4][2] = {
  {2, 0},
  {0, 2},
  {-2, 0},
  {0, -2}
};

int cur_maze_size = 0; // index of current maze size
int maze_size; // current size of maze
int cx, cy; // current coordinate of marker

inline int ind(int x, int y) {
  return x + y * maze_size;
}

/**
   returns possible directions from the target position
   @return the number of possible directions
   @param x, y --- target position
   @param rdirs --- return the possible directions
 */
int possible_dir(int x, int y, int* rdirs) {
  int ct = 0;
  for (int dir = 0; dir < 4; dir++) {
    int nx = x + DIR[dir][0];
    int ny = y + DIR[dir][1];
    if (nx > 0 && nx < maze_size - 1 &&
        ny > 0 && ny < maze_size - 1 &&
        maze[ind(nx, ny)] == WALL) {
      if (rdirs != NULL) rdirs[ct] = dir;
      ct++;
    }
  }
  return ct;
}

/**
   find a new position from the top left of the maze
   @return true if it found
   @param rx, ry --- the new position
 */
bool find_new_position(int* rx, int* ry) {
  for (int y = 1; y < maze_size - 1; y += 2) {
    for (int x = 1; x < maze_size - 1; x += 2) {
      if (maze[ind(x, y)] != WAY) continue;
      int ndirs = possible_dir(x, y, NULL);
      if (ndirs > 0) {
        *rx = x;
        *ry = y;
        return true;
      }
    }
  }
  return false;
}

void init_maze() {
  maze_size = MAZE_SIZE[cur_maze_size];
  memset(maze, WALL, maze_size * maze_size);
  int x = 1, y = 1;
  maze[ind(x, y)] = WAY;

  for (;;) {
    int dirs[4];
    int ndirs = possible_dir(x, y, dirs);
    if (ndirs > 0) {
      // OK, go to one of the dirctions
      int dir = dirs[rand() % ndirs];
      int dx = DIR[dir][0];
      int dy = DIR[dir][1];
      maze[ind(x + dx/2, y + dy/2)] = WAY;
      x += dx;
      y += dy;
      maze[ind(x, y)] = WAY;
    } else {
      // stucked in a local dead end, so find a new way
      if (!find_new_position(&x, &y)) break;
    }
  }
  // set the goal at the right bottom corner of the maze
  maze[ind(maze_size - 1, maze_size - 2)] = GOAL;
  // we start from the top right corner of the maze
  cx = cy = 1;
}

/* dump the maze for debug */
void dump_maze() {
  USBSerial.printf("\n");
  for (int y = 0; y < maze_size; y++) {
    for (int x = 0; x < maze_size; x++) {
      USBSerial.printf(maze[ind(x, y)] == WALL ? "*" : " ");
    }
    USBSerial.printf("\n");
  }
}

/* move the marker */
void move_draw(int dx, int dy) {
  int sz = 128 / (maze_size + 1);
  // erase it
  if (sz == 1) {
    M5.Lcd.drawPixel(cx, cy, MAROON);
  } else {
    M5.Lcd.fillRect(cx * sz, cy * sz, sz, sz, MAROON);
  }
  // move it
  cx += dx;
  cy += dy;
  // draw it
  if (sz == 1) {
    M5.Lcd.drawPixel(cx, cy, WHITE);
  } else {
    M5.Lcd.fillRect(cx * sz, cy * sz, sz, sz, WHITE);
  }
}

/* draw the maze */
void draw_maze() {
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.startWrite();
  int sz = 128 / (maze_size + 1);
  unsigned char* p = maze;
  if (sz == 1) {
    for (int y = 0; y < maze_size; y++) {
      for (int x = 0; x < maze_size; x++, p++) {
        M5.Lcd.drawPixel(x, y, *p == WALL ? DARKCYAN : BLACK);
      }
    }
  } else {
    for (int y = 0; y < maze_size; y++) {
      for (int x = 0; x < maze_size; x++, p++) {
        M5.Lcd.fillRect(x * sz, y * sz, sz, sz, *p == WALL ? DARKCYAN : BLACK);
      }
    }
  }
  M5.Lcd.endWrite();
  move_draw(0, 0); // draw the marker
}

enum { IDLE, RUNNING } mode;
int seed = 0;

void setup() {
  M5.begin(true, true, true, false);
  M5.Lcd.begin();
  M5.Lcd.setRotation(2);
  M5.Lcd.fillScreen(M5.Lcd.color565(0, 0, 32));
  M5.IMU.begin();

  // draw initial screen
  mode = IDLE;
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.drawString("2D Maze", 40, 30, 2);
  M5.Lcd.drawString("Click to Start", 25, 100, 2);
  M5.Lcd.setTextColor(YELLOW);
  M5.Lcd.drawString("Tilt: Move", 15, 70, 1);
  M5.Lcd.drawString("Click: Change Size", 15, 80, 1);
}

void loop() {
  M5.update();

  if (mode == IDLE) {
    seed++;
    // check click to start
    if (M5.Btn.wasPressed()) {
      srand(seed);
      init_maze();
      draw_maze();
      mode = RUNNING;
    }
    return;
  }

  // check click to change the size
  if (M5.Btn.wasPressed()) {
    cur_maze_size = (cur_maze_size + 1) % 4;
    init_maze();
    draw_maze();
  }

  // check tilt to move the marker
  float ax, ay, az;
  M5.IMU.getAccel(&ax, &ay, &az);
  const float TH = 0.2;
  if (ax > TH && maze[ind(cx - 1, cy)] != WALL) move_draw(-1, 0);
  if (ax < -TH && maze[ind(cx + 1, cy)] != WALL) move_draw(1, 0);
  if (ay < -TH && maze[ind(cx, cy - 1)] != WALL) move_draw(0, -1);
  if (ay > TH && maze[ind(cx, cy + 1)] != WALL) move_draw(0, 1);

  // check it arrives at the goal
  if (maze[ind(cx, cy)] == GOAL) {
    M5.Lcd.setTextColor(PINK, BLACK);
    M5.Lcd.drawString("Congratulations!", 10, 40, 2);
    M5.Lcd.drawString("Click to Restart", 10, 100, 2);
    mode = IDLE;
  }

  delay(66);
}
