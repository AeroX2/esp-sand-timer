#pragma once

#include <Arduino.h>

#include <vector>

#include "Layer_Background.h"
#include "MPU6050_6Axis_MotionApps_V6_12.h"
#include "RemoteDebug.h"
extern RemoteDebug Debug;

Quaternion q;
uint8_t fifoBuffer[64];
VectorFloat gravity;

class Grid;

class Particle {
 public:
  int x;
  int y;
  char type = ' ';
  bool movable = false;
  Particle(int x, int y) {
    this->x = x;
    this->y = y;
  }

  virtual void update(Grid* grid, VectorInt16 gravity){};
  virtual void draw(SMLayerBackground<rgb24, 0>* layer){};
};

class Grid {
 public:
  int width;
  int height;
  Grid(int width, int height) {
    this->width = width;
    this->height = height;
    internal_grid = new char[width * height];

    for (int x = 0; x < width; x++) {
      for (int y = 0; y < height; y++) {
        set(x, y, ' ');
      }
    }
  }

  char get(int x, int y) {
    if (x < 0 || x > width - 1 || y < 0 || y > height - 1) return ' ';
    return internal_grid[y * width + x];
  }
  void set(int x, int y, char particle) {
    if (x < 0 || x > width - 1 || y < 0 || y > height - 1) return;
    internal_grid[y * width + x] = particle;
  }

  void clear() {
    for (int x = 0; x < width; x++) {
      for (int y = 0; y < height; y++) {
        set(x, y, ' ');
      }
    }
  }

 private:
  char* internal_grid;
};

class Wall : public Particle {
 public:
  Wall(int x, int y) : Particle(x, y) { type = 'w'; };
  void draw(SMLayerBackground<rgb24, 0>* layer) {
    layer->drawPixel(x, y, {0, 192, 0});
  }
};

struct Coord {
  int x;
  int y;
};

class Sand : public Particle {
  Coord bounds[9][2] = {
      {{1, 0}, {0, 1}},  //
      {{0, 0}, {2, 0}},  //
      {{1, 0}, {2, 1}},  //
      {{0, 0}, {0, 2}},  //
      {{0, 0}, {0, 0}},  //
      {{2, 0}, {2, 2}},  //
      {{0, 1}, {1, 2}},  //
      {{0, 2}, {2, 2}},  //
      {{1, 2}, {2, 1}},  //
  };

 public:
  Sand(int x, int y) : Particle(x, y) {
    type = 's';
    movable = true;

    tx = x * 256;
    ty = y * 256;
  };
  void update(Grid* grid, VectorInt16 gravity) {
    vx += gravity.x;
    vy += gravity.y;
    // Serial.printf("Grav: %d, %d\n", gravity.x, gravity.y);
    // Serial.printf("Vel: %d, %d\n", vx, vy);

    float v2 = vx * vx + vy * vy;
    if (v2 > 65536) {                     // If v^2 > 65536, then v > 256
      float v = sqrt(v2);                 // Velocity vector magnitude
      vx = (int)(256.0 * (float)vx / v);  // Maintain heading &
      vy = (int)(256.0 * (float)vy / v);  // limit magnitude
    }

    tx += vx;
    ty += vy;

    int nx = tx / 256;
    int ny = ty / 256;

    if (nx < 0) {
      nx = 0;
      tx = 0;
      vx = 0;
    } else if (nx > grid->width - 1) {
      nx = grid->width - 1;
      tx = nx * 256;
      vx = 0;
    }

    if (ny < 0) {
      ny = 0;
      ty = 0;
      vy = 0;
    } else if (ny > grid->height - 1) {
      ny = grid->height - 1;
      ty = ny * 256;
      vy = 0;
    }

    if (grid->get(nx, ny) == ' ') {
      // No collision
      x = nx;
      y = ny;
      return;
    }

    int dx = nx - x;
    int dy = ny - y;
    if (dx == 0 && dy == 0) return;

    // TODO Fix this bug
    if (dx < -1 || dx > 1 || dy < -1 || dy > 1) return;

    Coord c1 = bounds[(dy + 1) * 3 + (dx + 1)][0];
    c1.x--;
    c1.y--;
    nx = c1.x + x;
    ny = c1.y + y;
    if (grid->get(nx, ny) == ' ') {
      x = nx;
      y = ny;
      return;
    }

    Coord c2 = bounds[(dy + 1) * 3 + (dx + 1)][1];
    c2.x--;
    c2.y--;
    nx = c2.x + x;
    ny = c2.y + y;
    if (grid->get(nx, ny) == ' ') {
      x = nx;
      y = ny;
      return;
    }

    if (dx != 0) {
      tx = x * 256;
      vx = 0;
    }

    if (dy != 0) {
      ty = y * 256;
      vy = 0;
    }
  }

  void draw(SMLayerBackground<rgb24, 0>* layer) {
    layer->drawPixel(x, y, {192, 192, 192});
  }

 private:
  int tx = 0;
  int ty = 0;
  int vx = 0;
  int vy = 0;
  int ax = 0;
  int ay = 0;
};

class Simulation {
 public:
  void init(int worldWidth, int worldHeight) {
    grid = new Grid(worldWidth, worldHeight);

    for (int x = 0; x < 5; x++) {
      for (int y = 0; y < 5; y++) {
        Particle* particle = new Sand(12 + x, 12 + y);
        particles.push_back(particle);
        grid->set(12 + x, 12 + y, 's');
      }
    }

    // particles.push_back(new Wall(10, 3));
    // grid->set(10, 3, 'w');
  }

  void update(VectorFloat gravity) {
    VectorFloat gravityNormalised = gravity.getNormalized();
    VectorInt16 gravityScaled = {(int16_t)(gravityNormalised.x * 256),
                                 (int16_t)(gravityNormalised.y * -256),
                                 (int16_t)(gravityNormalised.z * 256)};
    for (Particle* particle : particles) {
      int x = particle->x;
      int y = particle->y;
      particle->update(grid, gravityScaled);
      if (particle->x != x || particle->y != y) {
        grid->set(x, y, ' ');
        grid->set(particle->x, particle->y, 's');
      }
    }
  }

  void draw(SMLayerBackground<rgb24, 0>* layer) {
    layer->fillScreen({0x40, 0, 0});
    for (Particle* particle : particles) {
      particle->draw(layer);
    }
    layer->swapBuffers();
  }

  void updateAndDraw(SMLayerBackground<rgb24, 0>* layer, MPU6050* mpu) {
    if (millis() - lastUpdateMillis > 1000 / 60) {
      if (mpu != nullptr) {
        mpu->dmpGetCurrentFIFOPacket(fifoBuffer);
        mpu->dmpGetQuaternion(&q, fifoBuffer);
        mpu->dmpGetGravity(&gravity, &q);
      }
      update(gravity);
      lastUpdateMillis = millis();
    }
    draw(layer);
  }

 private:
  int lastUpdateMillis = 1000;
  std::vector<Particle*> particles;
  Grid* grid;
};
