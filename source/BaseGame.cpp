#include <radix/BaseGame.hpp>

#include <SDL2/SDL_timer.h>

#include <radix/env/Environment.hpp>
#include <radix/SoundManager.hpp>
#include <radix/simulation/Player.hpp>
#include <radix/simulation/Physics.hpp>
#include <radix/entities/Player.hpp>
#include <radix/env/ArgumentsParser.hpp>
#include <radix/env/GameConsole.hpp>

namespace radix {

Fps BaseGame::fps;

BaseGame::BaseGame() :
    config(),
    gameWorld(window),
    closed(false) {
  radix::Environment::init();
  config = Environment::getConfig();
  radix::ArgumentsParser::populateConfig(config);
  window.setConfig(config);
}

BaseGame::~BaseGame() {
}

void BaseGame::setup() {
  radix::GameConsole console;
  if (config.isConsoleEnabled()) {
    console.run(*this);
  }
  SoundManager::init();
  createWindow();
  initHook();
  customTriggerHook();

  std::unique_ptr<World> newWorld = std::make_unique<World>(*this);
  createWorld(*newWorld);

  lastUpdate = 0;
  lastRender = 0;

  setWorld(std::move(newWorld));
  // From this point on, this->world is the moved newWorld

  createScreenshotCallbackHolder();
  loadMap();

  screenRenderer = std::make_unique<ScreenRenderer>(*world, *renderer.get(), gameWorld);
  renderer->addRenderer(*screenRenderer);
}

bool BaseGame::isRunning() {
  return !closed;
}

World* BaseGame::getWorld() {
  return world.get();
}

void BaseGame::switchToOtherWorld(const std::string &name) {
  auto it = otherWorlds.find(name);
  if (it == otherWorlds.end()) {
    throw std::out_of_range("No otherworld by this name");
  }
  setWorld(std::move(it->second));
  otherWorlds.erase(it);
}

void BaseGame::clearOtherWorldList() {
  otherWorlds.clear();
}

ScreenRenderer* BaseGame::getScreenRenderer() {
  return screenRenderer.get();
}

GameWorld* BaseGame::getGameWorld() {
  return &gameWorld;
}

void BaseGame::preCycle() {
}

void BaseGame::update() {
  currentTime = SDL_GetTicks();
  int elapsedTime = currentTime - lastUpdate;
  SoundManager::update(world->getPlayer());
  world->update(TimeDelta::msec(elapsedTime));
  lastUpdate = currentTime;
}

void BaseGame::postCycle() {
  if (postCycleDeferred.size() > 0) {
    for (auto deferred : postCycleDeferred) {
      deferred();
    }
    postCycleDeferred.clear();
  }
}

void BaseGame::deferPostCycle(const std::function<void()> &deferred) {
  postCycleDeferred.push_back(deferred);
}

void BaseGame::processInput() { } /* to avoid pure virtual function */
void BaseGame::initHook() { }
void BaseGame::customTriggerHook() { }

void BaseGame::cleanUp() {
  setWorld({});
  window.close();
}

void BaseGame::render() {
  prepareCamera();
  renderer->render();
  gameWorld.getScreens()->clear();
  fps.countCycle();
  window.swapBuffers();
  lastRender = currentTime;
}

void BaseGame::prepareCamera() {
  world->camera->setPerspective();
  int viewportWidth, viewportHeight;
  window.getSize(&viewportWidth, &viewportHeight);
  world->camera->setAspect((float)viewportWidth / viewportHeight);
  const entities::Player &player = world->getPlayer();
  Vector3f headOffset(0, player.getScale().y / 2, 0);
  world->camera->setPosition(player.getPosition() + headOffset);
  world->camera->setOrientation(player.getHeadOrientation());
}

void BaseGame::close() {
  closed = true;
}

void BaseGame::createWorld(World &world) {
  onPreCreateWorld(world);
  world.setConfig(config);
  world.onCreate();
  { SimulationManager::Transaction simTransact = world.simulations.transact();
    simTransact.addSimulation<simulation::Player>(this);
    simTransact.addSimulation<simulation::Physics>(this);
  }
  world.initPlayer();
  onPostCreateWorld(world);
}

void BaseGame::setWorld(std::unique_ptr<World> &&newWorld) {
  if (world) {
    onPreStopWorld();
    world->onStop();
    onPostStopWorld();
    onPreDestroyWorld(*world);
    world->onDestroy();
    onPostDestroyWorld(*world);
  }
  world = std::move(newWorld);
  if (world) {
    // TODO move
    renderer = std::make_unique<Renderer>(*world);
    renderer->setViewport(&window);
    renderer->init();

    onPreStartWorld();
    world->onStart();
    onPostStartWorld();
  }
}

void BaseGame::loadMap() {
  XmlMapLoader mapLoader(*world, customTriggers);
  std::string map = config.getMap(), mapPath = config.getMapPath();
  if (map.length() > 0) {
    mapLoader.load(Environment::getDataDir() + map);
  } else if (mapPath.length() > 0) {
    mapLoader.load(mapPath);
  } else {
    mapLoader.load(Environment::getDataDir() + defaultMap);
  }
}

void BaseGame::createWindow() {
  window.create(windowTitle.c_str());
  if(config.getCursorVisibility()) {
    window.unlockMouse();
  } else {
    window.lockMouse();
  }
}

void BaseGame::createScreenshotCallbackHolder() {
  screenshotCallbackHolder =
    world->event.addObserver(InputSource::KeyReleasedEvent::Type, [this](const radix::Event &event) {
        const int key =  ((InputSource::KeyReleasedEvent &) event).key;
        if (key == SDL_SCANCODE_G) {
          this->window.printScreenToFile(radix::Environment::getDataDir() + "/screenshot.bmp");
        }
      });
}
} /* namespace radix */
