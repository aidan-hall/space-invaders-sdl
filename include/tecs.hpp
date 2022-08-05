#ifndef TECS_H
#define TECS_H

#include <bitset>
#include <cassert>
#include <cstdint>
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


struct Coordinator {

  std::unordered_map<std::type_index, ComponentId> componentIds;

  Entity nextEntity = 0;

  ComponentId nextComponentId;

  Coordinator() {
    // A Signature is a Component that every Entity implicitly has,
    // identifying what other Components it has.
    nextComponentId = -1;
    registerComponent<Signature>();
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

  template <typename Component>
  inline void addComponent(Entity e) {
    auto s = getComponent<Signature>(e);
    s.set(componentId<Component>());
  }

  template <typename Component>
  inline void removeComponent(Entity e) {
    auto s = getComponent<Signature>(e);
    s.reset(componentId<Component>());
  }

  template <typename Component>
  inline Component& getComponent(Entity e) {
    assert(e < nextEntity);
    assert(hasComponent<Component>(e));
    static std::vector<Component> components;
    if (components.size() <= e) {
      components.resize(e + 1);
    }

    return components[e];
  }

  template <typename Component>
  inline bool hasComponent(Entity e) {
    #ifndef NDEBUG
    if (componentId<Component>() == componentId<Signature>()) {
      return true;
    }
    #endif
    return getComponent<Signature>(e).test(componentId<Component>());
  }
};

} // namespace Tecs

#endif // TECS_H
