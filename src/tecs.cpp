#include <cassert>
#include <typeindex>
#include <tecs.hpp>
#include <algorithm>

using namespace Tecs;

System::System(const Signature &componentMask, Coordinator &coord) {
  id = coord.registerSystem(componentMask);
}
Coordinator::Coordinator() {
  // A Signature is a Component that every Entity implicitly has,
  // identifying what other Components it has.
  nextComponentId = -1;
  registerComponent<ComponentMask>();
}

SystemId Coordinator::registerSystem(const Signature &sig) {
  auto signatures = getComponents<ComponentMask>();

  std::vector<std::pair<Entity, ComponentMask>> entityMaskPairs;

  // Got to wait for C++23 for zip.
  Entity entity = 0;
  std::transform(signatures.begin(), signatures.end(), std::back_inserter(entityMaskPairs),
                 [&entity](ComponentMask mask) {
                   return std::pair{entity++, mask};
                 });
  auto s = systems.registerSystem(entityMaskPairs, sig);

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
  getComponent<ComponentMask>(e).reset();
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
  auto &s = getComponent<ComponentMask>(e);
  ComponentMask old = s;
  s.reset(c);

  for (SystemId system = 0; system < systems.systemSignatures.size(); ++system) {
    const auto &systemSignature = systems.systemSignatures[system];
    if (Tecs::SystemManager::isInteresting(old, systemSignature) &&
        !Tecs::SystemManager::isInteresting(s, systemSignature)) {
      auto &interests = systems.systemInterests[system];
      interests.erase(e);
    }
  }
}

void Coordinator::addComponent(Entity e, ComponentId c) {
    auto &s = getComponent<ComponentMask>(e);
    const auto old = s;
    s.set(c);

    for (SystemId system = 0; system < systems.systemSignatures.size(); ++system) {
      const auto &systemSignature = systems.systemSignatures[system];
      if (Tecs::SystemManager::isInteresting(s, systemSignature) &&
          !Tecs::SystemManager::isInteresting(old, systemSignature)) {
        auto &interests = systems.systemInterests[system];
        interests.insert(e);
      }
    }
}
