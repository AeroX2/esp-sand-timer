#pragma once

#include <Arduino.h>

#include <vector>

#include "Layer_Background.h"
#include "RemoteDebug.h"
extern RemoteDebug Debug;

#define WORLD_MULTIPLIER 3

class Grid;

class Particle {
 public:
  int x;
  int y;
  bool movable = false;
  Particle(int x, int y) {
    this->x = x;
    this->y = y;
  }

  virtual void update(Grid* grid){};
  virtual void draw(SMLayerBackground<rgb24, 0>* layer){};
};

class Grid {
 public:
  int width;
  int height;
  Grid(int width, int height) {
    this->width = width;
    this->height = height;
    internal_grid = new int8_t[width * height];

    for (int x = 0; x < width; x++) {
      for (int y = 0; y < height; y++) {
        set(x, y, 0);
      }
    }
  }

  int8_t get(int x, int y) { return internal_grid[y * width + x]; }
  void set(int x, int y, int count) { internal_grid[y * width + x] = count; }

  char check(int x, int y) {
    if (x < 0 || x > width - 1 || y < 0 || y > height - 1) return 'e';
    if (get(x, y) != 0) return 's';
    return 'n';
  }

  void clear() {
    for (int x = 0; x < width; x++) {
      for (int y = 0; y < height; y++) {
        set(x, y, 0);
      }
    }
  }

 private:
  int8_t* internal_grid;
};

class Wall : public Particle {
 public:
  Wall(int x, int y) : Particle(x, y){};
};

class Sand : public Particle {
 public:
  int vx = 0;
  int vy = 0;
  bool movable = true;
  Sand(int x, int y) : Particle(x, y){};
  void update(Grid* grid) {
    char test = grid->check(x, y + 1);
    if (test == 'n') {
      y++;
      return;
    }

    test = grid->check(x - 1, y + 1);
    if (test == 'n') {
      x--;
      y++;
      return;
    }

    test = grid->check(x + 1, y + 1);
    if (test == 'n') {
      x++;
      y++;
      return;
    }
  }
};

const rgb24 defaultBackgroundColor = {0x40, 0, 0};

class Simulation {
 public:
  void init(int displayWidth, int displayHeight) {
    worldGrid = new Grid(displayWidth * WORLD_MULTIPLIER,
                         displayHeight * WORLD_MULTIPLIER);
    displayGrid = new Grid(displayWidth, displayHeight);

    for (int x = 0; x < 10; x++) {
      for (int y = 0; y < 10; y++) {
        Particle* particle = new Sand(12 + x, 12 + y);
        particles.push_back(particle);
        worldGrid->set(x, y, 1);
      }
    }

    particles.push_back(new Wall(10, 3));
    worldGrid->set(10, 3, -1);
  }

  void update() {
    for (Particle* particle : particles) {
      particle->update(worldGrid);
    }
    worldGrid->clear();
    for (Particle* particle : particles) {
      worldGrid->set(particle->x, particle->y, particle->movable ? 1 : -1);
    }
  }

  void draw(SMLayerBackground<rgb24, 0>* layer) {
    layer->fillScreen(defaultBackgroundColor);

    displayGrid->clear();
    for (int x = 0; x < worldGrid->width; x++) {
      for (int y = 0; y < worldGrid->height; y++) {
        int dx = x / WORLD_MULTIPLIER;
        int dy = y / WORLD_MULTIPLIER;
        int particle = worldGrid->get(x, y);
        if (particle == -1) {
          displayGrid->set(dx, dy, -1);
        } else if (particle > 0) {
          displayGrid->set(dx, dy, displayGrid->get(dx, dy) + 1);
        }
      }
    }

    for (int x = 0; x < displayGrid->width; x++) {
      for (int y = 0; y < displayGrid->height; y++) {
        int pixelCount = displayGrid->get(x, y);
        if (pixelCount == 0) continue;

        // Assume this is a wall.
        if (pixelCount == -1) pixelCount = WORLD_MULTIPLIER;

        uint8_t pixelDensity = map(pixelCount, 0, WORLD_MULTIPLIER, 0, 255);

        layer->drawPixel(
            x, y, {pixelDensity, pixelDensity, pixelDensity});
      }
    }

    layer->swapBuffers();
  }

  void updateAndDraw(SMLayerBackground<rgb24, 0>* layer) {
    if (millis() - lastUpdateMillis > 1000 / 20) {
      update();
      draw(layer);
      lastUpdateMillis = millis();
    }
  }

 private:
  int lastUpdateMillis = 0;
  std::vector<Particle*> particles;
  Grid* worldGrid;
  Grid* displayGrid;
};
