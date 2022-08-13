#include <bitset>
#include <cassert>
#include <typeindex>
#include <iostream>

#include <tecs.hpp>

using namespace Tecs;

System::System(const Signature &sig, Coordinator &coord) {
  id = coord.registerSystem(sig);
}
Coordinator::Coordinator() {
  // A Signature is a Component that every Entity implicitly has,
  // identifying what other Components it has.
  nextComponentId = -1;
  registerComponent<Signature>();
}

SystemId Coordinator::registerSystem(const Signature &sig) {
  auto signatures = getComponents<Signature>();

  auto s = systems.registerSystem(signatures, sig);

  return s;
}
Entity Coordinator::newEntity() {
  Entity e;
  if (recycledEntities.empty()) {
    e = nextEntity++;
  } else {
    e = recycledEntities.front();
    recycledEntities.dequeue();
  }
  return e;
}
void Coordinator::destroyEntity(Entity e) {
  getComponent<Signature>(e).reset();
  for (auto &interest : systems.systemInterests) {
    interest.erase(e);
  }

  recycledEntities.enqueue(e);
}

void Coordinator::queueDestroyEntity(Entity e) {
  pendingDestructions.enqueue(e);
}

void Coordinator::destroyQueued() {
  while (!pendingDestructions.empty()) {
    Entity e = pendingDestructions.front();
    std::cout << "Destroying: " << e << std::endl;
    pendingDestructions.dequeue();
    destroyEntity(e);
  }
}

ComponentId Coordinator::registerComponent(const std::type_index& typeIndex) {
    assert(not componentIds.contains(typeIndex));

    const auto myComponentId = nextComponentId;
    componentIds[typeIndex] = myComponentId;
    nextComponentId += 1;

    return myComponentId;
}

void Coordinator::removeComponent(Entity e, ComponentId c) {
  auto &s = getComponent<Signature>(e);
  Signature old = s;
  s.reset(c);

  for (SystemId system = 0; system < systems.systemSignatures.size(); ++system) {
    const auto &systemSignature = systems.systemSignatures[system];
    if (systems.isInteresting(old, systemSignature) &&
        !systems.isInteresting(s, systemSignature)) {
      auto &interests = systems.systemInterests[system];
      interests.erase(e);
    }
  }
}

void Coordinator::addComponent(Entity e, ComponentId c) {
    auto &s = getComponent<Signature>(e);
    const Signature old = s;
    s.set(c);

    for (SystemId system = 0; system < systems.systemSignatures.size(); ++system) {
      const auto &systemSignature = systems.systemSignatures[system];
      if (systems.isInteresting(s, systemSignature) &&
          !systems.isInteresting(old, systemSignature)) {
        auto &interests = systems.systemInterests[system];
        interests.insert(e);
      }
    }
}
