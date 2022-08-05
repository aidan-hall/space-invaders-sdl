#ifndef TECS_H
#define TECS_H

#include <bitset>
#include <cassert>
#include <cstdint>
#include <tuple>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <vector>

// 'Inspired' by https://austinmorlan.com/posts/entity_component_system

namespace Tecs {

using Entity = std::size_t;
using ComponentId = std::uint8_t;
static constexpr std::size_t MAX_COMPONENTS = 64;
using Signature = std::bitset<MAX_COMPONENTS>;

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
  std::vector<std::vector<Entity>> systemInterests;

  SystemId registerSystem(const std::vector<Signature> &entitySignatures,
                          const Signature &sig) {
    systemInterests.push_back(deriveInterests(entitySignatures, sig));

    return nextSystem++;
  }

  // Derive which entities a System with the given Signature would be interested
  // in.
  static std::vector<Entity>
  deriveInterests(const std::vector<Signature> &entitySignatures,
                  const Signature &sig) {
    std::vector<Entity> interests;
    for (Entity i = 0; i < entitySignatures.size(); ++i) {
      if ((entitySignatures[i] & sig) == sig) {
        interests.push_back(i);
      }
    }

    return interests;
  }

  std::vector<Entity> interestsOf(SystemId id) { return systemInterests[id]; }
};

struct Coordinator {

  std::unordered_map<std::type_index, ComponentId> componentIds;

  Entity nextEntity = 0;

  ComponentId nextComponentId;

  SystemManager systems;
  std::vector<bool> systemsUpToDate;

  Coordinator() {
    // A Signature is a Component that every Entity implicitly has,
    // identifying what other Components it has.
    nextComponentId = -1;
    registerComponent<Signature>();
  }

  SystemId registerSystem(const Signature &sig) {
    auto signatures = getComponents<Signature>();

    return systems.registerSystem(signatures, sig);
  }

  SystemId registerSystem(const std::vector<ComponentId> &components) {
    return registerSystem(componentsSignature(components));
  }

  inline Entity newEntity() { return nextEntity++; }

  template <typename Component> ComponentId registerComponent() {

    auto ti = std::type_index(typeid(Component));
    assert(not componentIds.contains(ti));

    const auto myComponentId = nextComponentId;
    componentIds[ti] = myComponentId;
    nextComponentId += 1;

    return myComponentId;
  }

  template <typename Component> inline ComponentId componentId() {
    return componentIds[std::type_index(typeid(Component))];
  }

  template <typename Component> inline void addComponent(Entity e) {
    auto &s = getComponent<Signature>(e);
    auto c = componentId<Component>();
    s.set(c);
  }

  template <typename Component> inline void removeComponent(Entity e) {
    auto s = getComponent<Signature>(e);
    s.reset(componentId<Component>());
  }

  template <typename Component> inline std::vector<Component> &getComponents() {
    static std::vector<Component> components;
    return components;
  }

  template <typename Component> inline Component &getComponent(Entity e) {
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
