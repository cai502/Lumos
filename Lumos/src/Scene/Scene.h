#pragma once

#include "Core/Types.h"
#include "SceneGraph.h"
#include "Maths/Maths.h"
#include "Utilities/AssetManager.h"

#include "Events/Event.h"
#include "Events/ApplicationEvent.h"

#include <sol/forward.hpp>

DISABLE_WARNING_PUSH
DISABLE_WARNING_CONVERSION_TO_SMALLER_TYPE
#include <entt/entity/registry.hpp>
DISABLE_WARNING_POP

namespace Lumos
{
	class TimeStep;
	class Font;
	class Event;
	class Camera;
	class EntityManager;
	class Entity;

	namespace Graphics
	{
		struct Light;
		class GBuffer;
		class Material;
	}

	class LUMOS_EXPORT Scene
	{
	public:
		explicit Scene(const std::string& SceneName); //Called once at program start - all scene initialization should be done in 'OnInitialize'
		virtual ~Scene();

		// Called when scene is being activated, and will begin being rendered/updated.
		//	 - Initialize objects/physics here
		virtual void OnInit();

		// Called when scene is being swapped and will no longer be rendered/updated
		//	 - Remove objects/physics here
		//	   Note: Default action here automatically delete all game objects
		virtual void OnCleanupScene();

		virtual void Render3D()
		{
		}
		virtual void Render2D()
		{
		}

		// Update Scene Logic
		//   - Called once per frame and should contain all time-sensitive update logic
		//	   Note: This is time relative to seconds not milliseconds! (e.g. msec / 1000)
		virtual void OnUpdate(const TimeStep& timeStep);
		virtual void OnTick(){};
		virtual void OnImGui(){};
		virtual void OnEvent(Event& e);
		// Delete all contained Objects
		//    - This is the default action upon firing OnCleanupScene()
		void DeleteAllGameObjects();

		// The friendly name associated with this scene instance
		const std::string& GetSceneName() const
		{
			return m_SceneName;
		}

		void SetName(const std::string& name)
		{
			m_SceneName = name;
		}

		void SetScreenWidth(u32 width)
		{
			m_ScreenWidth = width;
		}
		void SetScreenHeight(u32 height)
		{
			m_ScreenHeight = height;
		}

		u32 GetScreenWidth() const
		{
			return m_ScreenWidth;
		}
		u32 GetScreenHeight() const
		{
			return m_ScreenHeight;
		}

		entt::registry& GetRegistry();

        void UpdateSceneGraph();

        void DuplicateEntity(Entity entity);
		void DuplicateEntity(Entity entity, Entity parent);
        Entity CreateEntity();
		Entity CreateEntity(const std::string& name);
    
        EntityManager* GetEntityManager() { return m_EntityManager.get(); }
		
		void SetHasCppClass(bool value) 
		{
			m_HasCppClass = value;
		}
		
		bool GetHasCppClass() const
		{
			return m_HasCppClass;
		}

		virtual void Serialise(const std::string& filePath, bool binary = false);
		virtual void Deserialise(const std::string& filePath, bool binary = false);

		template<typename Archive>
		void save(Archive& archive) const
		{
			archive(cereal::make_nvp("Version", 4));
			archive(cereal::make_nvp("Scene Name", m_SceneName));
		}
		
		template<typename Archive>
			void load(Archive& archive)
		{
			archive(cereal::make_nvp("Version", m_SceneSerialisationVersion));
			archive(cereal::make_nvp("Scene Name", m_SceneName));
		}

	protected:
		std::string m_SceneName;
		int m_SceneSerialisationVersion = 0 ;

		UniqueRef<EntityManager> m_EntityManager;

		u32 m_ScreenWidth;
		u32 m_ScreenHeight;
        
		SceneGraph m_SceneGraph;

		bool m_HasCppClass = true;

	private:
		NONCOPYABLE(Scene)

		bool OnWindowResize(WindowResizeEvent& e);

		friend class Entity;
	};
}
