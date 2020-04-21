#pragma once
#include "lmpch.h"
#include "Utilities/TSingleton.h"

namespace sol
{
	class state;
}

namespace Lumos
{
    class Scene;
	struct WindowProperties;

	class LUMOS_EXPORT LuaManager : public TSingleton<LuaManager>
	{
		friend class TSingleton<LuaManager>;
	public:
		LuaManager();
		~LuaManager();

		void OnInit();
        void OnUpdate(Scene* scene);

        void BindImGuiLua(sol::state* solState);
        void BindECSLua(sol::state* state);
        void BindMathsLua(sol::state* state);
        void BindLogLua(sol::state* state);
        void BindInputLua(sol::state* state);
		void BindSceneLua(sol::state* state);

		sol::state* GetState() const { return m_State; }

		WindowProperties LoadConfigFile(const String& file);

	private:
		sol::state* m_State;
	};
}
