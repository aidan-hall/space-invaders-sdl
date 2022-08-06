#ifndef TECS_H
#define TECS_H

#include <bitset>
#include <cassert>
#include <cstdint>
#include <queue>
#include <set>
#include <tuple>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <vector>

// 'Inspired' by https://austinmorlan.com/posts/entity_component_system

namespace Tecs {

using Entity = std::size_t;
using ComponentId = std::uint8_t;
// This value means a Signature should fit in a long/64-bit integer.
static constexpr std::size_t MAX_COMPONENTS = 64;
using Signature = std::bitset<MAX_COMPONENTS>;

  struct Coordinator;

inline Signature
componentsSignature(const std::vector<ComponentId> &components) {
  Signature s;
  for (auto &c : components) {
    s.set(c);
  }
  return s;
}

using SystemId = std::size_t;

struct SystemManager {
  SystemId nextSystem = 0;
  std::vector<std::set<Entity>> systemInterests;
  std::vector<Signature> systemSignatures;

  SystemId registerSystem(const std::vector<Signature> &entitySignatures,
                          const Signature &sig) {
    systemInterests.push_back(deriveInterests(entitySignatures, sig));
    systemSignatures.push_back(sig);

    return nextSystem++;
  }

  // Whether an entity with the given Signature would be interesting to the
  // given system.
  static inline bool isInteresting(const Signature &entity,
                                   const Signature &system) {
    return (entity & system) == system;
  }

  // Derive which entities a System with the given Signature would be interested
  // in.
  static std::set<Entity>
  deriveInterests(const std::vector<Signature> &entitySignatures,
                  const Signature &sig) {
    std::set<Entity> interests;
    for (Entity i = 0; i < entitySignatures.size(); ++i) {
      if (isInteresting(entitySignatures[i], sig)) {
        interests.insert(i);
      }
    }

    return interests;
  }

  void updateInterests(const std::vector<Signature> &entitySignatures,
                       SystemId system) {
    systemInterests[system] =
        deriveInterests(entitySignatures, systemSignatures[system]);
  }

  std::set<Entity> interestsOf(SystemId id) { return systemInterests[id]; }
};


struct System {
  SystemId id;


  virtual void run(const std::set<Entity> &entities, Coordinator &coord) = 0;

  System(const Signature &sig, Coordinator &coord);

};

struct Coordinator {

  std::unordered_map<std::type_index, ComponentId> componentIds;

  Entity nextEntity = 0;
  std::queue<Entity> recycledEntities;

  ComponentId nextComponentId;

  SystemManager systems;

  Coordinator();

  SystemId registerSystem(const Signature &sig);

  inline void registerSystem(System& sys, const Signature &sig) {
    sys.id = registerSystem(sig);
  }

  Entity newEntity();

  void destroyEntity(Entity e);

  ComponentId registerComponent(const std::type_index& typeIndex);

  template <typename Component> inline ComponentId registerComponent() {
    return registerComponent(std::type_index(typeid(Component)));
  }

  template <typename Component> inline ComponentId componentId() {
    const auto c = componentIds.at(std::type_index(typeid(Component)));
    return c;
  }

  void addComponent(Entity e, ComponentId c);
  
  template <typename Component> inline void addComponent(Entity e) {
    addComponent(e, componentId<Component>());
  }

  void removeComponent(Entity e, ComponentId c);


  template <typename Component> inline void removeComponent(Entity e) {
    removeComponent(e, componentId<Component>());
  }

  template <typename Component> inline std::vector<Component> &getComponents() {
    // The ugly foundation upon which this entire 'type safe' ECS framework is based.
    static std::vector<Component> components;
    return components;
  }

  template <typename Component> inline Component &getComponent(Entity e) {
    // TODO: Add assertion that Entity exists.
    assert(e < nextEntity);
    assert(hasComponent<Component>(e));

    std::vector<Component> &components = getComponents<Component>();
    if (components.size() <= e) {
      components.resize(e + 1);
    }

    return components[e];
  }

  template <typename Component> inline bool hasComponent(Entity e) {
    return getComponent<Signature>(e).test(componentId<Component>());
  }
};

// Every Entity implicitly has a Signature.
template <> inline bool Coordinator::hasComponent<Signature>(Entity e) {
  std::ignore = e;
  return true;
}


} // namespace Tecs

#endif // TECS_H
