#pragma once
#include "lmpch.h"
#include "App/Application.h"

#include <sol/forward.hpp>
#include <cereal/cereal.hpp>
namespace Lumos
{
	class Scene;

	class LUMOS_EXPORT ScriptComponent
	{
	public:
		ScriptComponent() = default;
		ScriptComponent(const std::string& fileName, Scene* scene);
		~ScriptComponent();

		void Init();
		void Update(float dt);
		void Reload();
		void Load(const std::string& fileName);

		void LoadScript(const std::string& fileName);

		const sol::environment& GetSolEnvironment() const
		{
			return *m_Env;
		}
		const std::string& GetFilePath() const
		{
			return m_FileName;
		}

		void SetFilePath(const std::string& path)
		{
			m_FileName = path;
		}

		const std::vector<std::string>& GetErrors() const
		{
			return m_Errors;
		}

		bool Loaded()
		{
			return m_Env != nullptr;
		}

		template<typename Archive>
		void save(Archive& archive) const
		{
			archive(cereal::make_nvp("FilePath", m_FileName));
		}

		template<typename Archive>
		void load(Archive& archive)
		{
			m_Scene = Application::Get().GetCurrentScene();
			archive(cereal::make_nvp("FilePath", m_FileName));
			Init();
		}

	private:
		Scene* m_Scene = nullptr;
		std::string m_FileName;

		std::vector<std::string> m_Errors;

		Ref<sol::environment> m_Env;
		Ref<sol::protected_function> m_UpdateFunc;
	};
}