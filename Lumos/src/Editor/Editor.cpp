#include "Precompiled.h"
#include "Editor.h"
#include "SceneWindow.h"
#include "ConsoleWindow.h"
#include "HierarchyWindow.h"
#include "InspectorWindow.h"
#include "ApplicationInfoWindow.h"
#include "GraphicsInfoWindow.h"
#include "TextEditWindow.h"
#include "AssetWindow.h"
#include "EditorCamera.h"
#include "Utilities/Timer.h"

#include "Core/Application.h"
#include "Core/OS/Input.h"
#include "Core/OS/FileSystem.h"
#include "Core/OS/OS.h"

#include "Core/Version.h"
#include "Core/Engine.h"
#include "Scene/Scene.h"
#include "Scene/SceneManager.h"
#include "Scene/Entity.h"
#include "Scene/EntityManager.h"
#include "Events/ApplicationEvent.h"

#include "Scene/Component/Components.h"
#include "Scripting/Lua/LuaScriptComponent.h"

#include "Physics/LumosPhysicsEngine/LumosPhysicsEngine.h"
#include "Physics/B2PhysicsEngine/B2PhysicsEngine.h"

#include "Graphics/Renderers/ForwardRenderer.h"
#include "Graphics/MeshFactory.h"
#include "Graphics/Layers/Layer3D.h"
#include "Graphics/Sprite.h"
#include "Graphics/AnimatedSprite.h"
#include "Graphics/Light.h"
#include "Graphics/API/Texture.h"
#include "Graphics/Camera/Camera.h"
#include "Graphics/Layers/LayerStack.h"
#include "Graphics/API/GraphicsContext.h"
#include "Graphics/Renderers/GridRenderer.h"
#include "Graphics/Renderers/DebugRenderer.h"
#include "Graphics/Model.h"
#include "Graphics/Environment.h"
#include "Scene/EntityFactory.h"

#include "ImGui/IconsMaterialDesignIcons.h"

#include <imgui/imgui_internal.h>
#include <imgui/plugins/ImGuizmo.h>
#include <imgui/plugins/ImGuiAl/button/imguial_button.h>
#include <imgui/plugins/ImTextEditor.h>

#include <imgui/plugins/ImFileBrowser.h>

static ImVec2 operator+(const ImVec2& a, const ImVec2& b)
{
	return ImVec2(a.x + b.x, a.y + b.y);
}

namespace Lumos
{
	Editor::Editor(Application* app, u32 width, u32 height)
		: m_Application(app)
		, m_IniFile("")
        , m_SelectedEntity(entt::null)
        , m_CopiedEntity(entt::null)
	{
	}
    
	Editor::~Editor()
	{
	}
    
	void Editor::OnInit()
	{
		LUMOS_PROFILE_FUNCTION();
        
#ifdef LUMOS_PLATFORM_IOS
        m_TempSceneSaveFilePath = OS::Instance()->GetAssetPath();
#else
        m_TempSceneSaveFilePath = ROOT_DIR"/bin/";
		const char* ini[] = {ROOT_DIR "/Editor.ini", ROOT_DIR "/Editor/Editor.ini"};
		bool fileFound = false;
		std::string filePath;
		for(int i = 0; i < IM_ARRAYSIZE(ini); ++i)
		{
			auto fexist = [](const char* f) -> bool {
				FILE* fp = fopen(f, "rb");
				return fp ? (static_cast<void>(fclose(fp)), 1) : 0;
			};
			if(fexist(ini[i]))
			{
				filePath = ini[i];
				m_IniFile = IniFile(filePath);
				// ImGui::GetIO().IniFilename = ini[i];
				fileFound = true;
				LoadEditorSettings();
				break;
			}
		}
        
		if(!fileFound)
		{
			filePath = ROOT_DIR "/editor.ini";
			FileSystem::WriteTextFile(filePath, "");
			m_IniFile = IniFile(filePath);
			AddDefaultEditorSettings();
			// ImGui::GetIO().IniFilename = "editor.ini";
		}
#endif
        
		m_EditorCamera =  CreateRef<Camera>(-20.0f,
                                            -40.0f,
                                            Maths::Vector3(-31.0f, 12.0f, 51.0f),
                                            60.0f,
                                            0.1f,
                                            1000.0f,
                                            (float)m_Application->GetWindowSize().x / (float)m_Application->GetWindowSize().y);
		m_CurrentCamera = m_EditorCamera.get();
        
		m_EditorCameraTransform.SetLocalPosition(Maths::Vector3(-31.0f, 12.0f, 51.0f));
		m_EditorCameraTransform.SetLocalOrientation({-20.0f, -40.0f, 0.0f});
        
		m_ComponentIconMap[typeid(Graphics::Light).hash_code()] = ICON_MDI_LIGHTBULB;
		m_ComponentIconMap[typeid(Camera).hash_code()] = ICON_MDI_CAMERA;
		m_ComponentIconMap[typeid(SoundComponent).hash_code()] = ICON_MDI_VOLUME_HIGH;
		m_ComponentIconMap[typeid(Graphics::Sprite).hash_code()] = ICON_MDI_IMAGE;
		m_ComponentIconMap[typeid(Maths::Transform).hash_code()] = ICON_MDI_VECTOR_LINE;
		m_ComponentIconMap[typeid(Physics2DComponent).hash_code()] = ICON_MDI_SQUARE_OUTLINE;
		m_ComponentIconMap[typeid(Physics3DComponent).hash_code()] = ICON_MDI_CUBE_OUTLINE;
		m_ComponentIconMap[typeid(Graphics::Model).hash_code()] = ICON_MDI_SHAPE;
		m_ComponentIconMap[typeid(LuaScriptComponent).hash_code()] = ICON_MDI_SCRIPT;
		m_ComponentIconMap[typeid(Graphics::Environment).hash_code()] = ICON_MDI_EARTH;
        
		m_Windows.emplace_back(CreateRef<ConsoleWindow>());
		m_Windows.emplace_back(CreateRef<SceneWindow>());
		m_Windows.emplace_back(CreateRef<InspectorWindow>());
        m_Windows.emplace_back(CreateRef<ApplicationInfoWindow>());
		m_Windows.emplace_back(CreateRef<HierarchyWindow>());
		m_Windows.emplace_back(CreateRef<GraphicsInfoWindow>());
		m_Windows.back()->SetActive(false);
#ifndef LUMOS_PLATFORM_IOS
		//m_Windows.emplace_back(CreateRef<AssetWindow>());
#endif
        
		for(auto& window : m_Windows)
			window->SetEditor(this);
        
        CreateGridRenderer();
        
		m_ShowImGuiDemo = false;
        
		m_SelectedEntity = entt::null;
		m_PreviewTexture = nullptr;
		
		ImGuizmo::SetGizmoSizeClipSpace(0.25f);
	}
    
	bool IsTextFile(const std::string& filePath)
	{
		LUMOS_PROFILE_FUNCTION();
		std::string extension = StringUtilities::GetFilePathExtension(filePath);
        
		if(extension == "txt" || extension == "glsl" || extension == "shader" || extension == "vert"
           || extension == "frag" || extension == "lua" || extension == "Lua")
			return true;
        
		return false;
	}
    
	bool IsAudioFile(const std::string& filePath)
	{
		LUMOS_PROFILE_FUNCTION();
		std::string extension = StringUtilities::GetFilePathExtension(filePath);
        
		if(extension == "ogg" || extension == "wav")
			return true;
        
		return false;
	}
    
    bool IsSceneFile(const std::string& filePath)
    {
		LUMOS_PROFILE_FUNCTION();
        std::string extension = StringUtilities::GetFilePathExtension(filePath);
        
        if(extension == "lsn")
            return true;
        
        return false;
    }
    
	bool IsModelFile(const std::string& filePath)
	{
		LUMOS_PROFILE_FUNCTION();
		std::string extension = StringUtilities::GetFilePathExtension(filePath);
        
		if(extension == "obj" || extension == "gltf" || extension == "glb" || extension == "fbx" || extension == "FBX")
			return true;
        
		return false;
	}
    
	void Editor::OnImGui()
	{
		LUMOS_PROFILE_FUNCTION();
		DrawMenuBar();
        
		BeginDockSpace(false);
        
		for(auto& window : m_Windows)
		{
			if(window->Active())
				window->OnImGui();
		}
        
		if(m_ShowImGuiDemo)
			ImGui::ShowDemoWindow(&m_ShowImGuiDemo);
        
		m_View2D = m_CurrentCamera->IsOrthographic();
        
		m_FileBrowserWindow.OnImGui();
        
		if(m_Application->GetEditorState() == EditorState::Preview)
			m_Application->GetSceneManager()->GetCurrentScene()->UpdateSceneGraph();
        
		EndDockSpace();
	}
    
	Graphics::RenderAPI StringToRenderAPI(const std::string& name)
	{
#ifdef LUMOS_RENDER_API_VULKAN
		if(name == "Vulkan")
			return Graphics::RenderAPI::VULKAN;
#endif
#ifdef LUMOS_RENDER_API_OPENGL
		if(name == "OpenGL")
			return Graphics::RenderAPI::OPENGL;
#endif
#ifdef LUMOS_RENDER_API_DIRECT3D
		if(name == "Direct3D11")
			return Graphics::RenderAPI::DIRECT3D;
#endif
        
		LUMOS_LOG_ERROR("Unsupported Graphics API");
        
		return Graphics::RenderAPI::OPENGL;
	}
    
	void Editor::OpenFile()
	{
		LUMOS_PROFILE_FUNCTION();
		m_FileBrowserWindow.SetCallback(BIND_FILEBROWSER_FN(Editor::FileOpenCallback));
		m_FileBrowserWindow.Open();
	}
    
	void Editor::DrawMenuBar()
	{
		LUMOS_PROFILE_FUNCTION();
		if(ImGui::BeginMainMenuBar())
		{
			if(ImGui::BeginMenu("File"))
			{
				if(ImGui::MenuItem("Exit"))
				{
					Application::Get().SetAppState(AppState::Closing);
				}
                
				if(ImGui::MenuItem("Open File"))
				{
					m_FileBrowserWindow.SetCallback(BIND_FILEBROWSER_FN(Editor::FileOpenCallback));
					m_FileBrowserWindow.Open();
				}
                
				if(ImGui::MenuItem("New Scene"))
				{
					auto scene = new Scene("New Scene");
					scene->SetHasCppClass(false);
					m_Application->GetSceneManager()->EnqueueScene(scene);
					m_Application->GetSceneManager()->SwitchScene((int)(m_Application->GetSceneManager()->GetScenes().size()) - 1);
				}
                
				if(ImGui::BeginMenu("Style"))
				{
					if(ImGui::MenuItem("Dark", ""))
					{
						m_Theme = ImGuiHelpers::Dark;
						ImGuiHelpers::SetTheme(ImGuiHelpers::Dark);
					}
					if(ImGui::MenuItem("Black", ""))
					{
						m_Theme = ImGuiHelpers::Black;
						ImGuiHelpers::SetTheme(ImGuiHelpers::Black);
					}
					if(ImGui::MenuItem("Grey", ""))
					{
						m_Theme = ImGuiHelpers::Grey;
						ImGuiHelpers::SetTheme(ImGuiHelpers::Grey);
					}
					if(ImGui::MenuItem("Light", ""))
					{
						m_Theme = ImGuiHelpers::Light;
						ImGuiHelpers::SetTheme(ImGuiHelpers::Light);
					}
					if(ImGui::MenuItem("Cherry", ""))
					{
						m_Theme = ImGuiHelpers::Cherry;
						ImGuiHelpers::SetTheme(ImGuiHelpers::Cherry);
					}
					if(ImGui::MenuItem("Blue", ""))
					{
						m_Theme = ImGuiHelpers::Blue;
						ImGuiHelpers::SetTheme(ImGuiHelpers::Blue);
					}
					if(ImGui::MenuItem("Cinder", ""))
					{
						m_Theme = ImGuiHelpers::Cinder;
						ImGuiHelpers::SetTheme(ImGuiHelpers::Cinder);
					}
					if(ImGui::MenuItem("Classic", ""))
					{
						m_Theme = ImGuiHelpers::Classic;
						ImGuiHelpers::SetTheme(ImGuiHelpers::Classic);
					}
					if(ImGui::MenuItem("ClassicDark", ""))
					{
						m_Theme = ImGuiHelpers::ClassicDark;
						ImGuiHelpers::SetTheme(ImGuiHelpers::ClassicDark);
					}
					if(ImGui::MenuItem("ClassicLight", ""))
					{
						m_Theme = ImGuiHelpers::ClassicLight;
						ImGuiHelpers::SetTheme(ImGuiHelpers::ClassicLight);
					}
					ImGui::EndMenu();
				}
                
				ImGui::EndMenu();
			}
			if(ImGui::BeginMenu("Edit"))
			{
				//TODO
				// if(ImGui::MenuItem("Undo", "CTRL+Z"))
				// {
				// }
				// if(ImGui::MenuItem("Redo", "CTRL+Y", false, false))
				// {
				// } // Disabled item
				// ImGui::Separator();
                
				bool enabled = m_SelectedEntity != entt::null;
                
				if(ImGui::MenuItem("Cut", "CTRL+X", false, enabled))
				{
					m_CopiedEntity = m_SelectedEntity;
					m_CutCopyEntity = true;
				}
                
				if(ImGui::MenuItem("Copy", "CTRL+C", false, enabled))
				{
					m_CopiedEntity = m_SelectedEntity;
					m_CutCopyEntity = false;
				}
                
				enabled = m_CopiedEntity != entt::null;
                
				if(ImGui::MenuItem("Paste", "CTRL+V", false, enabled))
				{
					m_Application->GetCurrentScene()->DuplicateEntity({ m_CopiedEntity, m_Application->GetCurrentScene() });
					if(m_CutCopyEntity)
					{
                        if(m_CopiedEntity == m_SelectedEntity)
                            m_SelectedEntity = entt::null;
						Entity(m_CopiedEntity, m_Application->GetCurrentScene()).Destroy();
					}
				}
                
				ImGui::EndMenu();
			}
            
			if(ImGui::BeginMenu("Windows"))
			{
				for(auto& window : m_Windows)
				{
					if(ImGui::MenuItem(window->GetName().c_str(), "", &window->Active(), true))
					{
						window->SetActive(true);
					}
				}
                
				if(ImGui::MenuItem("ImGui Demo", "", &m_ShowImGuiDemo, true))
				{
					m_ShowImGuiDemo = true;
				}
                
				ImGui::EndMenu();
			}
            
			if(ImGui::BeginMenu("Scenes"))
			{
				auto scenes = Application::Get().GetSceneManager()->GetSceneNames();
                
				for(size_t i = 0; i < scenes.size(); i++)
				{
					auto name = scenes[i];
					if(ImGui::MenuItem(name.c_str()))
					{
						Application::Get().GetSceneManager()->SwitchScene(name);
					}
				}
                
				ImGui::EndMenu();
			}
            
			if(ImGui::BeginMenu("Entity"))
			{
				auto scene = m_Application->GetSceneManager()->GetCurrentScene();
                
				if(ImGui::MenuItem("CreateEmpty"))
				{
					scene->CreateEntity();
				}
                
				if(ImGui::MenuItem("Cube"))
				{
					auto entity = scene->CreateEntity("Cube");
					entity.AddComponent<Graphics::Model>(Ref<Graphics::Mesh>(Graphics::CreatePrimative(Graphics::PrimitiveType::Cube)),Graphics::PrimitiveType::Cube);
				}
                
				if(ImGui::MenuItem("Sphere"))
				{
					auto entity = scene->CreateEntity("Sphere");
					entity.AddComponent<Graphics::Model>(Ref<Graphics::Mesh>(Graphics::CreatePrimative(Graphics::PrimitiveType::Sphere)),Graphics::PrimitiveType::Sphere);
				}
                
				if(ImGui::MenuItem("Pyramid"))
				{
					auto entity = scene->CreateEntity("Pyramid");
					entity.AddComponent<Graphics::Model>(Ref<Graphics::Mesh>(Graphics::CreatePrimative(Graphics::PrimitiveType::Pyramid)),Graphics::PrimitiveType::Pyramid);
				}
                
				if(ImGui::MenuItem("Plane"))
				{
					auto entity = scene->CreateEntity("Plane");
					entity.AddComponent<Graphics::Model>(Ref<Graphics::Mesh>(Graphics::CreatePrimative(Graphics::PrimitiveType::Plane)),Graphics::PrimitiveType::Plane);
				}
                
				if(ImGui::MenuItem("Cylinder"))
				{
					auto entity = scene->CreateEntity("Cylinder");
					entity.AddComponent<Graphics::Model>(Ref<Graphics::Mesh>(Graphics::CreatePrimative(Graphics::PrimitiveType::Cylinder)),Graphics::PrimitiveType::Cylinder);
				}
                
				if(ImGui::MenuItem("Capsule"))
				{
					auto entity = scene->CreateEntity("Capsule");
					entity.AddComponent<Graphics::Model>(Ref<Graphics::Mesh>(Graphics::CreatePrimative(Graphics::PrimitiveType::Capsule)),Graphics::PrimitiveType::Capsule);
				}
                
				if(ImGui::MenuItem("Terrain"))
				{
					auto entity = scene->CreateEntity("Terrain");
					entity.AddComponent<Graphics::Model>(Ref<Graphics::Mesh>(Graphics::CreatePrimative(Graphics::PrimitiveType::Terrain)),Graphics::PrimitiveType::Terrain);
				}
                
				if(ImGui::MenuItem("Light Cube"))
				{
					EntityFactory::AddLightCube(m_Application->GetSceneManager()->GetCurrentScene(), Maths::Vector3(0.0f), Maths::Vector3(0.0f));
				}
                
				ImGui::EndMenu();
			}
            
			if(ImGui::BeginMenu("Graphics"))
			{
				if(ImGui::MenuItem("Compile Shaders"))
				{
					RecompileShaders();
				}
				ImGui::EndMenu();
			}
			
			if(ImGui::BeginMenu("About"))
			{
				auto& version = Lumos::LumosVersion;
				ImGui::Text("Version : %d.%d.%d", version.major, version.minor, version.patch);
				ImGui::Separator();
                
				std::string githubMenuText = ICON_MDI_GITHUB_BOX " Github";
				if(ImGui::MenuItem(githubMenuText.c_str()))
				{
#ifdef LUMOS_PLATFORM_WINDOWS
					// TODO
					//ShellExecuteA( NULL, "open",  "https://www.github.com/jmorton06/Lumos", NULL, NULL, SW_SHOWNORMAL );
#else
#	ifndef LUMOS_PLATFORM_IOS
					system("open https://www.github.com/jmorton06/Lumos");
#	endif
#endif
				}
                
				ImGui::EndMenu();
			}
            
			ImGui::SameLine((ImGui::GetWindowContentRegionMax().x / 2.0f) - (1.5f * (ImGui::GetFontSize() + ImGui::GetStyle().ItemSpacing.x)));
            
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.2f, 0.7f, 0.0f));
            
			if(m_Application->GetEditorState() == EditorState::Next)
				m_Application->SetEditorState(EditorState::Paused);
            
			bool selected;
			{
				selected = m_Application->GetEditorState() == EditorState::Play;
				if(selected)
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.28f, 0.56f, 0.9f, 1.0f));
                
				if(ImGui::Button(ICON_MDI_PLAY))
                {
                    m_Application->GetSystem<LumosPhysicsEngine>()->SetPaused(selected);
                    m_Application->GetSystem<B2PhysicsEngine>()->SetPaused(selected);
                    m_Application->SetEditorState(selected ? EditorState::Preview : EditorState::Play);
                    
                    m_SelectedEntity = entt::null;
					if(selected)
						LoadCachedScene();
					else
                    {
                        CacheScene();
                        m_Application->GetCurrentScene()->OnInit();
                    }
                }
                
				ImGuiHelpers::Tooltip("Play");
                
				if(selected)
					ImGui::PopStyleColor();
			}
            
			ImGui::SameLine();
            
			{
				selected = m_Application->GetEditorState() == EditorState::Paused;
				if(selected)
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.28f, 0.56f, 0.9f, 1.0f));
                
				if(ImGui::Button(ICON_MDI_PAUSE))
					m_Application->SetEditorState(selected ? EditorState::Play : EditorState::Paused);
                
				ImGuiHelpers::Tooltip("Pause");
                
				if(selected)
					ImGui::PopStyleColor();
			}
            
			ImGui::SameLine();
            
			{
				selected = m_Application->GetEditorState() == EditorState::Next;
				if(selected)
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.28f, 0.56f, 0.9f, 1.0f));
                
				if(ImGui::Button(ICON_MDI_STEP_FORWARD))
					m_Application->SetEditorState(EditorState::Next);
                
				ImGuiHelpers::Tooltip("Next");
                
				if(selected)
					ImGui::PopStyleColor();
			}
            
			ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 240.0f);
			
			static Engine::Stats stats = {};
			static float timer = 1.1f;
			timer += Engine::GetTimeStep().GetMillis();
			
			if(timer > 1.0f)
			{
				timer = 0.0f;
				stats = Engine::Get().Statistics();
			}
			
			ImGui::Text("%.2f ms (%i FPS)", stats.FrameTime * 1000.0f, stats.FramesPerSecond);
			
			ImGui::SameLine();
            
			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::GetColorU32(ImGuiCol_TitleBg));
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(7, 2));
            
			bool setNewValue = false;
			static std::string RenderAPI = "";
			
			auto renderAPI = (Graphics::RenderAPI)m_Application->RenderAPI;
            
			bool needsRestart = false;
			if(renderAPI != Graphics::GraphicsContext::GetRenderAPI())
			{
				needsRestart = true;
			}
            
			switch(renderAPI)
			{
#ifdef LUMOS_RENDER_API_OPENGL
                case Graphics::RenderAPI::OPENGL:
				RenderAPI = "OpenGL";
				break;
#endif
                
#ifdef LUMOS_RENDER_API_VULKAN
                case Graphics::RenderAPI::VULKAN:
				RenderAPI = "Vulkan";
				break;
#endif
                
#ifdef LUMOS_RENDER_API_DIRECT3D
                case DIRECT3D:
				RenderAPI = "Direct3D";
				break;
#endif
                default:
				break;
			}
            
			int numSupported = 0;
#ifdef LUMOS_RENDER_API_OPENGL
			numSupported++;
#endif
#ifdef LUMOS_RENDER_API_VULKAN
			numSupported++;
#endif
#ifdef LUMOS_RENDER_API_DIRECT3D11
			numSupported++;
#endif
			const char* api[] = {"OpenGL", "Vulkan", "Direct3D11"};
			const char* current_api = RenderAPI.c_str();
			if(needsRestart)
				RenderAPI = "*" + RenderAPI;
            
			ImGui::PushItemWidth(-1.0f);
			if(ImGui::BeginCombo(
                                 "", current_api, 0)) // The second parameter is the label previewed before opening the combo.
			{
				for(int n = 0; n < numSupported; n++)
				{
					bool is_selected = (current_api == api[n]);
					if(ImGui::Selectable(api[n], current_api))
					{
						setNewValue = true;
						current_api = api[n];
					}
					if(is_selected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
            
			if(needsRestart)
				ImGuiHelpers::Tooltip("Restart needed to switch Render API");
            
			if(setNewValue)
			{
				m_Application->RenderAPI = int(StringToRenderAPI(current_api));
			}
            
			ImGui::PopStyleColor(2);
			ImGui::PopStyleVar();
            
			ImGui::EndMainMenuBar();
		}
	}
    
	static const float identityMatrix[16] =
    {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f};
    
	void Editor::OnImGuizmo()
	{
		LUMOS_PROFILE_FUNCTION();
		Maths::Matrix4 view = m_EditorCameraTransform.GetWorldMatrix().Inverse();//m_CurrentCamera->GetViewMatrix();
		Maths::Matrix4 proj = m_CurrentCamera->GetProjectionMatrix();
        
#ifdef LUMOS_RENDER_API_VULKAN
		if(Graphics::GraphicsContext::GetRenderAPI() == Graphics::RenderAPI::VULKAN)
			proj.m11_ *= -1.0f;
#endif
        
		view = view.Transpose();
		proj = proj.Transpose();
        
#ifdef USE_IMGUIZMO_GRID
		if(m_ShowGrid && !m_CurrentCamera->IsOrthographic())
            ImGuizmo::DrawGrid(Maths::ValuePointer(view),
                               Maths::ValuePointer(proj), identityMatrix, 120.f);
#endif
        
		if(m_SelectedEntity == entt::null || m_ImGuizmoOperation == 4)
			return;
        
		if(m_ShowGizmos)
		{
			ImGuizmo::SetDrawlist();
			ImGuizmo::SetOrthographic(m_CurrentCamera->IsOrthographic());
            
			auto& registry = m_Application->GetSceneManager()->GetCurrentScene()->GetRegistry();
			auto transform = registry.try_get<Maths::Transform>(m_SelectedEntity);
			if(transform != nullptr)
			{
				Maths::Matrix4 model = transform->GetWorldMatrix();
				model = model.Transpose();
                
				float snapAmount[3] = {m_SnapAmount, m_SnapAmount, m_SnapAmount};
				float delta[16];
                
				ImGuizmo::Manipulate(Maths::ValuePointer(view),
                                     Maths::ValuePointer(proj),
                                     static_cast<ImGuizmo::OPERATION>(m_ImGuizmoOperation),
                                     ImGuizmo::LOCAL,
                                     Maths::ValuePointer(model),
                                     delta,
                                     m_SnapQuizmo ? snapAmount : nullptr);
                
				if(ImGuizmo::IsUsing())
				{
					if(static_cast<ImGuizmo::OPERATION>(m_ImGuizmoOperation) == ImGuizmo::OPERATION::SCALE)
					{
						auto mat = Maths::Matrix4(delta).Transpose();
						transform->SetLocalScale(transform->GetLocalScale() * mat.Scale());
					}
					else
					{
						auto mat = Maths::Matrix4(delta).Transpose() * transform->GetLocalMatrix();
						transform->SetLocalTransform(mat);
                        
						auto physics2DComponent = registry.try_get<Physics2DComponent>(m_SelectedEntity);
                        
						if(physics2DComponent)
						{
							physics2DComponent->GetRigidBody()->SetPosition(
                                                                            {mat.Translation().x, mat.Translation().y});
						}
						else
						{
							auto physics3DComponent = registry.try_get<Physics3DComponent>(m_SelectedEntity);
							if(physics3DComponent)
							{
								physics3DComponent->GetRigidBody()->SetPosition(mat.Translation());
								physics3DComponent->GetRigidBody()->SetOrientation(mat.Rotation());
							}
						}
					}
				}
			}
		}
	}
    
	void Editor::BeginDockSpace(bool infoBar)
	{
		LUMOS_PROFILE_FUNCTION();
		static bool p_open = true;
		static bool opt_fullscreen_persistant = true;
		static ImGuiDockNodeFlags opt_flags = ImGuiDockNodeFlags_NoWindowMenuButton | ImGuiDockNodeFlags_NoCloseButton;
		bool opt_fullscreen = opt_fullscreen_persistant;
        
		// We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
		// because it would be confusing to have two docking targets within each others.
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;
		if(opt_fullscreen)
		{
			ImGuiViewport* viewport = ImGui::GetMainViewport();
            
			auto pos = viewport->Pos;
			auto size = viewport->Size;
			bool menuBar = true;
			if(menuBar)
			{
				const float infoBarSize = 19.0f;
				pos.y += infoBarSize;
				size.y -= infoBarSize;
			}
            
			if(infoBar)
			{
				const float infoBarSize = 24.0f;
				pos.y += infoBarSize;
				size.y -= infoBarSize;
			}
            
			ImGui::SetNextWindowPos(pos);
			ImGui::SetNextWindowSize(size);
			ImGui::SetNextWindowViewport(viewport->ID);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
			window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize
                | ImGuiWindowFlags_NoMove;
			window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
		}
        
		// When using ImGuiDockNodeFlags_PassthruDockspace, DockSpace() will render our background and handle the
		// pass-thru hole, so we ask Begin() to not render a background.
		if(opt_flags & ImGuiDockNodeFlags_DockSpace)
			window_flags |= ImGuiWindowFlags_NoBackground;
        
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("MyDockspace", &p_open, window_flags);
		ImGui::PopStyleVar();
        
		if(opt_fullscreen)
			ImGui::PopStyleVar(2);
        
        ImGuiID DockspaceID = ImGui::GetID("MyDockspace");
        
        if (!ImGui::DockBuilderGetNode(DockspaceID))
		{
            ImGui::DockBuilderRemoveNode(DockspaceID); // Clear out existing layout
            ImGui::DockBuilderAddNode(DockspaceID); // Add empty node
            ImGui::DockBuilderSetNodeSize(DockspaceID, ImGui::GetIO().DisplaySize);
            
            ImGuiID dock_main_id = DockspaceID;
            ImGuiID DockBottom = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.3f, nullptr, &dock_main_id);
            ImGuiID DockLeft = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.2f, nullptr, &dock_main_id);
            ImGuiID DockRight = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.20f, nullptr, &dock_main_id);
            
            ImGuiID DockLeftChild = ImGui::DockBuilderSplitNode(DockLeft, ImGuiDir_Down, 0.875f, nullptr, &DockLeft);
            ImGuiID DockRightChild = ImGui::DockBuilderSplitNode(DockRight, ImGuiDir_Down, 0.875f, nullptr, &DockRight);
            ImGuiID DockingLeftDownChild = ImGui::DockBuilderSplitNode(DockLeftChild, ImGuiDir_Down, 0.06f, nullptr, &DockLeftChild);
            ImGuiID DockingRightDownChild = ImGui::DockBuilderSplitNode(DockRightChild, ImGuiDir_Down, 0.06f, nullptr, &DockRightChild);
            
            ImGuiID DockBottomChild = ImGui::DockBuilderSplitNode(DockBottom, ImGuiDir_Down, 0.2f, nullptr, &DockBottom);
            ImGuiID DockingBottomLeftChild = ImGui::DockBuilderSplitNode(DockBottomChild, ImGuiDir_Left, 0.5f, nullptr, &DockBottomChild);
            ImGuiID DockingBottomRightChild = ImGui::DockBuilderSplitNode(DockBottomChild, ImGuiDir_Right, 0.5f, nullptr, &DockBottomChild);
            
            ImGuiID DockMiddle = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.8f, nullptr, &dock_main_id);
            
            ImGui::DockBuilderDockWindow("###scene", DockMiddle);
            ImGui::DockBuilderDockWindow("###inspector", DockRight);
            ImGui::DockBuilderDockWindow("###console", DockingBottomLeftChild);
            ImGui::DockBuilderDockWindow("###profiler", DockingBottomLeftChild);
            ImGui::DockBuilderDockWindow("Assets", DockingBottomRightChild);
            ImGui::DockBuilderDockWindow("Dear ImGui Demo", DockLeft);
            ImGui::DockBuilderDockWindow("GraphicsInfo", DockLeft);
            ImGui::DockBuilderDockWindow("ApplicationInfo", DockLeft);
            ImGui::DockBuilderDockWindow("###hierarchy", DockLeft);
            
			ImGui::DockBuilderFinish(DockspaceID);
		}
        
		// Dockspace
		ImGuiIO& io = ImGui::GetIO();
		if(io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
		{
			ImGui::DockSpace(DockspaceID, ImVec2(0.0f, 0.0f), opt_flags);
		}
	}
    
	void Editor::EndDockSpace()
	{
		ImGui::End();
	}
    
	void Editor::OnNewScene(Scene* scene)
	{
		LUMOS_PROFILE_FUNCTION();
		m_SelectedEntity = entt::null;
        
		m_EditorCameraTransform.SetLocalPosition(Maths::Vector3(-31.0f, 12.0f, 51.0f));
		m_EditorCameraTransform.SetLocalOrientation({-20.0f, -40.0f, 0.0f});
        
		for(auto window : m_Windows)
		{
			window->OnNewScene(scene);
		}
        
        std::string Configuration;
        std::string Platform;
        std::string RenderAPI;
        std::string dash = " - ";
        
#ifdef LUMOS_DEBUG
        Configuration = "Debug";
#else
        Configuration = "Release";
#endif
        
#ifdef LUMOS_PLATFORM_WINDOWS
        Platform = "Windows";
#elif LUMOS_PLATFORM_LINUX
        Platform = "Linux";
#elif LUMOS_PLATFORM_MACOS
        Platform = "MacOS";
#elif LUMOS_PLATFORM_IOS
        Platform = "iOS";
#endif
        
        switch(Graphics::GraphicsContext::GetRenderAPI())
        {
#ifdef LUMOS_RENDER_API_OPENGL
            case Graphics::RenderAPI::OPENGL:
            RenderAPI = "OpenGL";
            break;
#endif
            
#ifdef LUMOS_RENDER_API_VULKAN
#    if defined(LUMOS_PLATFORM_MACOS) || defined(LUMOS_PLATFORM_IOS)
            case Graphics::RenderAPI::VULKAN:
            RenderAPI = "Vulkan ( MoltenVK )";
            break;
#    else
            case Graphics::RenderAPI::VULKAN:
            RenderAPI = "Vulkan";
            break;
#    endif
#endif
            
#ifdef LUMOS_RENDER_API_DIRECT3D
            case DIRECT3D:
            RenderAPI = "Direct3D";
            break;
#endif
            default:
            break;
        }
        
        std::stringstream Title;
        Title << Platform << dash << RenderAPI << dash << Configuration << dash << scene->GetSceneName() << dash << Application::Get().GetWindow()->GetTitle();
        
        Application::Get().GetWindow()->SetWindowTitle(Title.str());
	}
    
	void Editor::Draw3DGrid()
	{
		LUMOS_PROFILE_FUNCTION();
#if 1
		if(!m_GridRenderer)
		{
			return;
		}
        
		DebugRenderer::DrawHairLine(Maths::Vector3(-5000.0f, 0.0f, 0.0f), Maths::Vector3(5000.0f, 0.0f, 0.0f), Maths::Vector4(1.0f, 0.0f, 0.0f,1.0f));
		DebugRenderer::DrawHairLine(Maths::Vector3(0.0f, -5000.0f, 0.0f), Maths::Vector3(0.0f, 5000.0f, 0.0f), Maths::Vector4(0.0f, 1.0f, 0.0f,1.0f));
		DebugRenderer::DrawHairLine(Maths::Vector3(0.0f, 0.0f, -5000.0f), Maths::Vector3(0.0f, 0.0f, 5000.0f), Maths::Vector4(0.0f, 0.0f, 1.0f,1.0f));
		
		
		m_GridRenderer->BeginScene(Application::Get().GetSceneManager()->GetCurrentScene(), m_EditorCamera.get(), &m_EditorCameraTransform);
		m_GridRenderer->RenderScene(Application::Get().GetSceneManager()->GetCurrentScene());
#endif
	}
    
	void Editor::Draw2DGrid(ImDrawList* drawList,
                            const ImVec2& cameraPos,
                            const ImVec2& windowPos,
                            const ImVec2& canvasSize,
                            const float factor,
                            const float thickness)
	{
		LUMOS_PROFILE_FUNCTION();
		static const auto graduation = 10;
		float GRID_SZ = canvasSize.y * 0.5f / factor;
		const ImVec2& offset = {
			canvasSize.x * 0.5f - cameraPos.x * GRID_SZ, canvasSize.y * 0.5f + cameraPos.y * GRID_SZ};
        
		ImU32 GRID_COLOR = IM_COL32(200, 200, 200, 40);
		float gridThickness = 1.0f;
        
		const auto& gridColor = GRID_COLOR;
		auto smallGraduation = GRID_SZ / graduation;
		const auto& smallGridColor = IM_COL32(100, 100, 100, smallGraduation);
        
		for(float x = -GRID_SZ; x < canvasSize.x + GRID_SZ; x += GRID_SZ)
		{
			auto localX = floorf(x + fmodf(offset.x, GRID_SZ));
			drawList->AddLine(
                              ImVec2{localX, 0.0f} + windowPos, ImVec2{localX, canvasSize.y} + windowPos, gridColor, gridThickness);
            
			if(smallGraduation > 5.0f)
			{
				for(int i = 1; i < graduation; ++i)
				{
					const auto graduation = floorf(localX + smallGraduation * i);
					drawList->AddLine(ImVec2{graduation, 0.0f} + windowPos,
                                      ImVec2{graduation, canvasSize.y} + windowPos,
                                      smallGridColor,
                                      1.0f);
				}
			}
		}
        
		for(float y = -GRID_SZ; y < canvasSize.y + GRID_SZ; y += GRID_SZ)
		{
			auto localY = floorf(y + fmodf(offset.y, GRID_SZ));
			drawList->AddLine(
                              ImVec2{0.0f, localY} + windowPos, ImVec2{canvasSize.x, localY} + windowPos, gridColor, gridThickness);
            
			if(smallGraduation > 5.0f)
			{
				for(int i = 1; i < graduation; ++i)
				{
					const auto graduation = floorf(localY + smallGraduation * i);
					drawList->AddLine(ImVec2{0.0f, graduation} + windowPos,
                                      ImVec2{canvasSize.x, graduation} + windowPos,
                                      smallGridColor,
                                      1.0f);
				}
			}
		}
	}
    
	void Editor::OnEvent(Event& e)
	{
		LUMOS_PROFILE_FUNCTION();
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<WindowResizeEvent>(BIND_EVENT_FN(Editor::OnWindowResize));
        
		m_Application->OnEvent(e);
	}
    
	Maths::Ray Editor::GetScreenRay(int x, int y, Camera* camera, int width, int height)
	{
		LUMOS_PROFILE_FUNCTION();
		if(!camera)
			return Maths::Ray();
        
		float screenX = (float)x / (float)width;
		float screenY = (float)y / (float)height;
        
		bool flipY = false;
        
#ifdef LUMOS_RENDER_API_OPENGL
		if(Graphics::GraphicsContext::GetRenderAPI() == Graphics::RenderAPI::OPENGL)
			flipY = true;
#endif
		return camera->GetScreenRay(screenX, screenY, m_EditorCameraTransform.GetWorldMatrix().Inverse(), flipY);
	}
    
	void Editor::OnUpdate(const TimeStep& ts)
	{
		LUMOS_PROFILE_FUNCTION();
        if(m_Application->GetEditorState() == EditorState::Preview)
        {
			auto& registry = m_Application->GetSceneManager()->GetCurrentScene()->GetRegistry();
			
			if(Application::Get().GetSceneActive())
			{
				const Maths::Vector2 mousePos = Input::GetInput()->GetMousePosition();
				
				m_EditorCameraController.HandleMouse(m_EditorCameraTransform, ts.GetMillis(), mousePos.x, mousePos.y);
				m_EditorCameraController.HandleKeyboard(m_EditorCameraTransform, ts.GetMillis());
                
				if(Input::GetInput()->GetKeyPressed(InputCode::Key::F))
				{
					if(registry.valid(m_SelectedEntity))
					{
						auto transform = registry.try_get<Maths::Transform>(m_SelectedEntity);
						if(transform)
							FocusCamera(transform->GetWorldPosition(), 2.0f, 2.0f);
					}
				}
			}
			
			if(Input::GetInput()->GetKeyHeld(InputCode::Key::O))
			{
				FocusCamera(Maths::Vector3(0.0f, 0.0f, 0.0f), 2.0f, 2.0f);
			}
			
			if(m_TransitioningCamera)
			{
				if(m_CameraTransitionStartTime < 0.0f)
					m_CameraTransitionStartTime = ts.GetElapsedMillis();
				
				float focusProgress =
                    Maths::Min((ts.GetElapsedMillis() - m_CameraTransitionStartTime) / m_CameraTransitionSpeed, 1.f);
				Maths::Vector3 newCameraPosition = m_CameraStartPosition.Lerp(m_CameraDestination, focusProgress);
				m_EditorCameraTransform.SetLocalPosition(newCameraPosition);
                
				if(m_EditorCameraTransform.GetLocalPosition().Equals(m_CameraDestination))
					m_TransitioningCamera = false;
			}
			
			if(!Input::GetInput()->GetMouseHeld(InputCode::MouseKey::ButtonRight) && !ImGuizmo::IsUsing())
			{
				if(Input::GetInput()->GetKeyPressed(InputCode::Key::Q))
				{
					SetImGuizmoOperation(4);
				}
                
				if(Input::GetInput()->GetKeyPressed(InputCode::Key::W))
				{
					SetImGuizmoOperation(0);
				}
				
				if(Input::GetInput()->GetKeyPressed(InputCode::Key::E))
				{
					SetImGuizmoOperation(1);
				}
				
				if(Input::GetInput()->GetKeyPressed(InputCode::Key::R))
				{
					SetImGuizmoOperation(2);
				}
				
				if(Input::GetInput()->GetKeyPressed(InputCode::Key::T))
				{
					SetImGuizmoOperation(3);
				}
				
				if(Input::GetInput()->GetKeyPressed(InputCode::Key::Y))
				{
					ToggleSnap();
				}
			}
            
			if((Input::GetInput()->GetKeyHeld(InputCode::Key::LeftSuper) || (Input::GetInput()->GetKeyHeld(InputCode::Key::LeftControl)) )) 
			{
				if(Input::GetInput()->GetKeyPressed(InputCode::Key::S))
					Application::Get().GetSceneManager()->GetCurrentScene()->Serialise(ROOT_DIR "/Sandbox/res/scenes/", false);
                
				if(Input::GetInput()->GetKeyPressed(InputCode::Key::O))
					Application::Get().GetSceneManager()->GetCurrentScene()->Deserialise(ROOT_DIR "/Sandbox/res/scenes/", false);
                
                if(Input::GetInput()->GetKeyPressed(InputCode::Key::X))
                {
                    m_CopiedEntity = m_SelectedEntity;
                    m_CutCopyEntity = true;
                }
                
                if(Input::GetInput()->GetKeyPressed(InputCode::Key::C))
                {
                    m_CopiedEntity = m_SelectedEntity;
                    m_CutCopyEntity = false;
                }
                
                if(Input::GetInput()->GetKeyPressed(InputCode::Key::V))
                {
                    m_Application->GetCurrentScene()->DuplicateEntity({ m_CopiedEntity, m_Application->GetCurrentScene() });
                    if(m_CutCopyEntity)
                    {
                        if(m_CopiedEntity == m_SelectedEntity)
                            m_SelectedEntity = entt::null;
                        Entity(m_CopiedEntity, m_Application->GetCurrentScene()).Destroy();
                    }
                }
			}
            
            m_EditorCameraTransform.SetWorldMatrix(Maths::Matrix4());
		}
	}
    
	void Editor::BindEventFunction()
	{
		LUMOS_PROFILE_FUNCTION();
		m_Application->GetWindow()->SetEventCallback(BIND_EVENT_FN(Editor::OnEvent));
	}
    
	void Editor::FocusCamera(const Maths::Vector3& point, float distance, float speed)
	{
		LUMOS_PROFILE_FUNCTION();
		if(m_CurrentCamera->IsOrthographic())
		{
            m_EditorCameraTransform.SetLocalPosition(point);
			//m_CurrentCamera->SetScale(distance / 2.0f);
		}
		else
		{
			m_TransitioningCamera = true;
            
			m_CameraDestination = point + m_EditorCameraTransform.GetForwardDirection() * distance;
			m_CameraTransitionStartTime = -1.0f;
			m_CameraTransitionSpeed = 1.0f / speed;
            m_CameraStartPosition = m_EditorCameraTransform.GetLocalPosition();
		}
	}
    
	bool Editor::OnWindowResize(WindowResizeEvent& e)
	{
		return false;
	}
    
	void Editor::RecompileShaders()
	{
		LUMOS_PROFILE_FUNCTION();
		LUMOS_LOG_INFO("Recompiling shaders");
        
#ifdef LUMOS_RENDER_API_VULKAN
#	ifdef LUMOS_PLATFORM_WINDOWS
		// std::string filePath = ROOT_DIR"/Lumos/res/EngineShaders/CompileShadersWindows.bat";
		// system(filePath.c_str());
#	elif LUMOS_PLATFORM_MACOS
		std::string filePath = ROOT_DIR "/Lumos/res/EngineShaders/CompileShadersMac.sh";
		system(filePath.c_str());
#	endif
#endif
	}
    
	void Editor::DebugDraw()
	{
		LUMOS_PROFILE_FUNCTION();
		auto& registry = Application::Get().GetSceneManager()->GetCurrentScene()->GetRegistry();
        
		if(m_DebugDrawFlags & EditorDebugFlags::MeshBoundingBoxes)
		{
			auto group = registry.group<Graphics::Model>(entt::get<Maths::Transform>);
            
			for(auto entity : group)
			{
				const auto& [model, trans] = group.get<Graphics::Model, Maths::Transform>(entity);
				auto& meshes = model.GetMeshes();
				for(auto mesh : meshes)
				{
					if(mesh->GetActive())
					{
						auto& worldTransform = trans.GetWorldMatrix();
						auto bbCopy = mesh->GetBoundingBox()->Transformed(worldTransform);
						DebugRenderer::DebugDraw(bbCopy, Maths::Vector4(0.1f, 0.9f, 0.1f, 0.4f), true);
					}
				}
			}
		}
        
		if(m_DebugDrawFlags & EditorDebugFlags::SpriteBoxes)
		{
			auto group = registry.group<Graphics::Sprite>(entt::get<Maths::Transform>);
            
			for(auto entity : group)
			{
				const auto& [sprite, trans] = group.get<Graphics::Sprite, Maths::Transform>(entity);
                
				{
					auto& worldTransform = trans.GetWorldMatrix();
                    
					auto bb =
						Maths::BoundingBox(Maths::Rect(sprite.GetPosition(), sprite.GetPosition() + sprite.GetScale()));
					bb.Transform(trans.GetWorldMatrix());
					DebugRenderer::DebugDraw(bb, Maths::Vector4(0.1f, 0.9f, 0.1f, 0.4f), true);
				}
			}
		}
        
		if(m_DebugDrawFlags & EditorDebugFlags::CameraFrustum)
		{
			auto cameraGroup = registry.group<Camera>(entt::get<Maths::Transform>);
            
			for(auto entity : cameraGroup)
			{
				const auto& [camera, trans] = cameraGroup.get<Camera, Maths::Transform>(entity);
                
				{
					DebugRenderer::DebugDraw(camera.GetFrustum(trans.GetWorldMatrix().Inverse()), Maths::Vector4(0.9f));
				}
			}
		}
        
		if(registry.valid(m_SelectedEntity)  && Application::Get().GetEditorState() == EditorState::Preview)
		{
			auto transform = registry.try_get<Maths::Transform>(m_SelectedEntity);
            
			auto model = registry.try_get<Graphics::Model>(m_SelectedEntity);
			if(transform && model)
			{
				auto& meshes = model->GetMeshes();
				for(auto mesh : meshes)
				{
					if(mesh->GetActive())
					{
						auto& worldTransform = transform->GetWorldMatrix();
						auto bbCopy = mesh->GetBoundingBox()->Transformed(worldTransform);
						DebugRenderer::DebugDraw(bbCopy, Maths::Vector4(0.1f, 0.9f, 0.1f, 0.4f), true);
					}
				}
			}
            
			auto sprite = registry.try_get<Graphics::Sprite>(m_SelectedEntity);
			if(transform && sprite)
			{
				{
					auto& worldTransform = transform->GetWorldMatrix();
                    
					auto bb = Maths::BoundingBox(
                                                 Maths::Rect(sprite->GetPosition(), sprite->GetPosition() + sprite->GetScale()));
					bb.Transform(worldTransform);
					DebugRenderer::DebugDraw(bb, Maths::Vector4(0.1f, 0.9f, 0.1f, 0.4f), true);
				}
			}
			
			auto animSprite = registry.try_get<Graphics::AnimatedSprite>(m_SelectedEntity);
			if(transform && animSprite)
			{
				{
					auto& worldTransform = transform->GetWorldMatrix();
                    
					auto bb = Maths::BoundingBox(Maths::Rect(animSprite->GetPosition(), animSprite->GetPosition() + animSprite->GetScale()));
					bb.Transform(worldTransform);
					DebugRenderer::DebugDraw(bb, Maths::Vector4(0.1f, 0.9f, 0.1f, 0.4f), true);
				}
			}
            
			auto camera = registry.try_get<Camera>(m_SelectedEntity);
			if(camera && transform)
			{
				DebugRenderer::DebugDraw(camera->GetFrustum(transform->GetWorldMatrix().Inverse()), Maths::Vector4(0.9f));
			}
            
			auto light = registry.try_get<Graphics::Light>(m_SelectedEntity);
			if(light && transform)
			{
				DebugRenderer::DebugDraw(light, transform->GetWorldOrientation(), Maths::Vector4(light->Colour.ToVector3(), 0.2f));
			}
            
			auto sound = registry.try_get<SoundComponent>(m_SelectedEntity);
			if(sound)
			{
				DebugRenderer::DebugDraw(sound->GetSoundNode(), Maths::Vector4(0.8f, 0.8f, 0.8f, 0.2f));
			}
		}
	}
    
	void Editor::SelectObject(const Maths::Ray& ray)
	{
		LUMOS_PROFILE_FUNCTION();
		auto& registry = Application::Get().GetSceneManager()->GetCurrentScene()->GetRegistry();
		float closestEntityDist = Maths::M_INFINITY;
		entt::entity currentClosestEntity = entt::null;
        
		auto group = registry.group<Graphics::Model>(entt::get<Maths::Transform>);
        
		static Timer timer;
		static float timeSinceLastSelect = 0.0f;
        
		for(auto entity : group)
		{
			const auto& [model, trans] = group.get<Graphics::Model, Maths::Transform>(entity);
            
			auto& meshes = model.GetMeshes();
            
			for(auto mesh : meshes)
			{
				if(mesh->GetActive())
				{
					auto& worldTransform = trans.GetWorldMatrix();
                    
					auto bbCopy = mesh->GetBoundingBox()->Transformed(worldTransform);
					float dist = ray.HitDistance(bbCopy);
                    
					if(dist < Maths::M_INFINITY)
					{
						if(dist < closestEntityDist)
						{
							closestEntityDist = dist;
							currentClosestEntity = entity;
						}
					}
				}
			}
		}
        
		if(m_SelectedEntity != entt::null)
		{
			if(m_SelectedEntity == currentClosestEntity)
			{
				if(timer.GetElapsedS() - timeSinceLastSelect < 1.0f)
				{
					auto& trans = registry.get<Maths::Transform>(m_SelectedEntity);
					auto& model = registry.get<Graphics::Model>(m_SelectedEntity);
                    auto bb = model.GetMeshes().front()->GetBoundingBox()->Transformed(trans.GetWorldMatrix());
                    
					FocusCamera(trans.GetWorldPosition(), (bb.max_ - bb.min_).Length());
				}
				else
				{
					currentClosestEntity = entt::null;
				}
			}
            
			timeSinceLastSelect = timer.GetElapsedS();
			m_SelectedEntity = currentClosestEntity;
			return;
		}
        
		auto spriteGroup = registry.group<Graphics::Sprite>(entt::get<Maths::Transform>);
        
		for(auto entity : spriteGroup)
		{
			const auto& [sprite, trans] = spriteGroup.get<Graphics::Sprite, Maths::Transform>(entity);
            
			auto& worldTransform = trans.GetWorldMatrix();
			auto bb = Maths::BoundingBox(Maths::Rect(sprite.GetPosition(), sprite.GetPosition() + sprite.GetScale()));
			bb.Transform(trans.GetWorldMatrix());
			float dist = ray.HitDistance(bb);
            
			if(dist < Maths::M_INFINITY)
			{
				if(dist < closestEntityDist)
				{
					closestEntityDist = dist;
					currentClosestEntity = entity;
				}
			}
		}
		
		auto animSpriteGroup = registry.group<Graphics::AnimatedSprite>(entt::get<Maths::Transform>);
        
		for(auto entity : animSpriteGroup)
		{
			const auto& [sprite, trans] = animSpriteGroup.get<Graphics::AnimatedSprite, Maths::Transform>(entity);
            
			auto& worldTransform = trans.GetWorldMatrix();
			auto bb = Maths::BoundingBox(Maths::Rect(sprite.GetPosition(), sprite.GetPosition() + sprite.GetScale()));
			bb.Transform(trans.GetWorldMatrix());
			float dist = ray.HitDistance(bb);
            
			if(dist < Maths::M_INFINITY)
			{
				if(dist < closestEntityDist)
				{
					closestEntityDist = dist;
					currentClosestEntity = entity;
				}
			}
		}
        
		if(m_SelectedEntity != entt::null)
		{
			if(m_SelectedEntity == currentClosestEntity)
			{
				auto& trans = registry.get<Maths::Transform>(m_SelectedEntity);
				auto& sprite = registry.get<Graphics::Sprite>(m_SelectedEntity);
				auto bb =
					Maths::BoundingBox(Maths::Rect(sprite.GetPosition(), sprite.GetPosition() + sprite.GetScale()));
                
				FocusCamera(trans.GetWorldPosition(), (bb.max_ - bb.min_).Length());
			}
		}
        
		m_SelectedEntity = currentClosestEntity;
	}
    
	void Editor::OpenTextFile(const std::string& filePath)
	{
		LUMOS_PROFILE_FUNCTION();
		std::string physicalPath;
		if(!VFS::Get()->ResolvePhysicalPath(filePath, physicalPath))
		{
			LUMOS_LOG_ERROR("Failed to Load Lua script {0}", filePath);
			return;
		}
        
		for(int i = 0; i < int(m_Windows.size()); i++)
		{
			EditorWindow* w = m_Windows[i].get();
			if(w->GetSimpleName() == "TextEdit")
			{
				m_Windows.erase(m_Windows.begin() + i);
				break;
			}
		}
        
		m_Windows.emplace_back(CreateRef<TextEditWindow>(physicalPath));
		m_Windows.back()->SetEditor(this);
	}
    
	void Editor::RemoveWindow(EditorWindow* window)
	{
		LUMOS_PROFILE_FUNCTION();
		for(int i = 0; i < int(m_Windows.size()); i++)
		{
			EditorWindow* w = m_Windows[i].get();
			if(w == window)
			{
				m_Windows.erase(m_Windows.begin() + i);
				return;
			}
		}
	}
    
	void Editor::ShowPreview()
	{
		LUMOS_PROFILE_FUNCTION();
		ImGui::Begin("Preview");
		if(m_PreviewTexture)
			ImGuiHelpers::Image(m_PreviewTexture.get(), {200, 200});
		ImGui::End();
	}
    
	void Editor::OnRender()
	{
		LUMOS_PROFILE_FUNCTION();
		//DrawPreview();
        
		if(m_Application->GetEditorState() == EditorState::Preview && m_ShowGrid && !m_EditorCamera->IsOrthographic())
			Draw3DGrid();
	}
    
	void Editor::DrawPreview()
	{
		LUMOS_PROFILE_FUNCTION();
		if(!m_PreviewTexture)
		{
			m_PreviewTexture = Ref<Graphics::Texture2D>(Graphics::Texture2D::Create());
			m_PreviewTexture->BuildTexture(Graphics::TextureFormat::RGBA32, 200, 200, false, false);
            
			m_PreviewRenderer = CreateRef<Graphics::ForwardRenderer>(200, 200, false);
			m_PreviewSphere = Ref<Graphics::Mesh>(Graphics::CreateSphere());
            
			m_PreviewRenderer->SetRenderTarget(m_PreviewTexture.get(), true);
		}
        
		Maths::Matrix4 proj = Maths::Matrix4::Perspective(0.1f, 10.0f, 200.0f / 200.0f, 60.0f);
		Maths::Matrix4 view = Maths::Matrix3x4(Maths::Vector3(0.0f, 0.0f, 3.0f),
                                               Maths::Quaternion::EulerAnglesToQuaternion(0.0f, 0.0f, 0.0f),
                                               Maths::Vector3(1.0f))
            .Inverse()
            .ToMatrix4();
		m_PreviewRenderer->Begin();
		m_PreviewRenderer->BeginScene(proj, view);
		m_PreviewRenderer->SubmitMesh(m_PreviewSphere.get(), nullptr, Maths::Matrix4(), Maths::Matrix4());
        m_PreviewRenderer->SetSystemUniforms(m_PreviewRenderer->GetShader().get());
		m_PreviewRenderer->Present();
		m_PreviewRenderer->End();
	}
    
	void Editor::FileOpenCallback(const std::string& filePath)
	{
		LUMOS_PROFILE_FUNCTION();
		if(IsTextFile(filePath))
			OpenTextFile(filePath);
		else if(IsModelFile(filePath))
		{
			Entity modelEntity = m_Application->GetSceneManager()->GetCurrentScene()->GetEntityManager()->Create();
			modelEntity.AddComponent<Graphics::Model>(filePath);
			m_SelectedEntity = modelEntity.GetHandle();
		}
		else if(IsAudioFile(filePath))
		{
			//AssetsManager::Sounds()->LoadAsset(StringUtilities::GetFileName(filePath), filePath);
            
			auto soundNode = Ref<SoundNode>(SoundNode::Create());
			//soundNode->SetSound(AssetsManager::Sounds()->Get(StringUtilities::GetFileName(filePath)).get());
			soundNode->SetVolume(1.0f);
			soundNode->SetPosition(Maths::Vector3(0.1f, 10.0f, 10.0f));
			soundNode->SetLooping(true);
			soundNode->SetIsGlobal(false);
			soundNode->SetPaused(false);
			soundNode->SetReferenceDistance(1.0f);
			soundNode->SetRadius(30.0f);
            
			auto& registry = m_Application->GetSceneManager()->GetCurrentScene()->GetRegistry();
			entt::entity e = registry.create();
			registry.emplace<SoundComponent>(e, soundNode);
			m_SelectedEntity = e;
		}
        else if(IsSceneFile(filePath))
        {
            m_Application->GetSceneManager()->EnqueueSceneFromFile(filePath);
            m_Application->GetSceneManager()->SwitchScene((int)(m_Application->GetSceneManager()->GetScenes().size()) - 1);
        }
	}
    
	void Editor::SaveEditorSettings()
	{
		LUMOS_PROFILE_FUNCTION();
		m_IniFile.SetOrAdd("ShowGrid", m_ShowGrid);
		m_IniFile.SetOrAdd("ShowGizmos", m_ShowGizmos);
		m_IniFile.SetOrAdd("ShowViewSelected", m_ShowViewSelected);
		m_IniFile.SetOrAdd("TransitioningCamera", m_TransitioningCamera);
		m_IniFile.SetOrAdd("ShowImGuiDemo", m_ShowImGuiDemo);
		m_IniFile.SetOrAdd("SnapAmount", m_SnapAmount);
		m_IniFile.SetOrAdd("SnapQuizmo", m_SnapQuizmo);
		m_IniFile.SetOrAdd("DebugDrawFlags", m_DebugDrawFlags);
		m_IniFile.SetOrAdd("PhysicsDebugDrawFlags", Application::Get().GetSystem<LumosPhysicsEngine>()->GetDebugDrawFlags());
		m_IniFile.SetOrAdd("PhysicsDebugDrawFlags2D", Application::Get().GetSystem<B2PhysicsEngine>()->GetDebugDrawFlags());
		m_IniFile.SetOrAdd("Theme", (int)m_Theme);
		m_IniFile.Rewrite();
	}
    
	void Editor::AddDefaultEditorSettings()
	{
		LUMOS_PROFILE_FUNCTION();
		m_IniFile.Add("ShowGrid", m_ShowGrid);
		m_IniFile.Add("ShowGizmos", m_ShowGizmos);
		m_IniFile.Add("ShowViewSelected", m_ShowViewSelected);
		m_IniFile.Add("TransitioningCamera", m_TransitioningCamera);
		m_IniFile.Add("ShowImGuiDemo", m_ShowImGuiDemo);
		m_IniFile.Add("SnapAmount", m_SnapAmount);
		m_IniFile.Add("SnapQuizmo", m_SnapQuizmo);
		m_IniFile.Add("DebugDrawFlags", m_DebugDrawFlags);
		m_IniFile.Add("PhysicsDebugDrawFlags", 0);
		m_IniFile.Set("PhysicsDebugDrawFlags2D", 0);
		m_IniFile.Set("Theme", (int)m_Theme);
		m_IniFile.Rewrite();
	}
    
	void Editor::LoadEditorSettings()
	{
		LUMOS_PROFILE_FUNCTION();
		m_ShowGrid = m_IniFile.GetOrDefault("ShowGrid", m_ShowGrid);
		m_ShowGizmos = m_IniFile.GetOrDefault("ShowGizmos", m_ShowGizmos);
		m_ShowViewSelected = m_IniFile.GetOrDefault("ShowViewSelected", m_ShowViewSelected);
		m_TransitioningCamera = m_IniFile.GetOrDefault("TransitioningCamera", m_TransitioningCamera);
		m_ShowImGuiDemo = m_IniFile.GetOrDefault("ShowImGuiDemo", m_ShowImGuiDemo);
		m_SnapAmount = m_IniFile.GetOrDefault("SnapAmount", m_SnapAmount);
		m_SnapQuizmo = m_IniFile.GetOrDefault("SnapQuizmo", m_SnapQuizmo);
		m_DebugDrawFlags = m_IniFile.GetOrDefault("DebugDrawFlags", m_DebugDrawFlags);
		m_Theme = ImGuiHelpers::Theme(m_IniFile.GetOrDefault("Theme", (int)m_Theme));
		Application::Get().GetSystem<LumosPhysicsEngine>()->SetDebugDrawFlags(m_IniFile.GetOrDefault("PhysicsDebugDrawFlags", 0));
		Application::Get().GetSystem<B2PhysicsEngine>()->SetDebugDrawFlags(m_IniFile.GetOrDefault("PhysicsDebugDrawFlags2D", 0));
		
		ImGuiHelpers::SetTheme(m_Theme);
	}
    
	const char* Editor::GetIconFontIcon(const std::string& filePath)
	{ 
		LUMOS_PROFILE_FUNCTION();
		if(IsTextFile(filePath))
		{
			return ICON_MDI_FILE_XML;
		}
		else if(IsModelFile(filePath))
		{
			return ICON_MDI_SHAPE;
		}
		else if(IsAudioFile(filePath))
		{
			return ICON_MDI_FILE_MUSIC;
		}
        
		return ICON_MDI_FILE;
	}
    
	void Editor::CreateGridRenderer()
	{
		LUMOS_PROFILE_FUNCTION();
		if(!m_GridRenderer)
			m_GridRenderer = CreateRef<Graphics::GridRenderer>(u32(Application::Get().m_SceneViewWidth), u32(Application::Get().m_SceneViewHeight));
	}
    
	const Ref<Graphics::GridRenderer>& Editor::GetGridRenderer()
	{
		LUMOS_PROFILE_FUNCTION();
		//if(!m_GridRenderer)
		//  m_GridRenderer = CreateRef<Graphics::GridRenderer>(u32(Application::Get().m_SceneViewWidth), u32(Application::Get().m_SceneViewHeight), true);
		return m_GridRenderer;
	}
    
	void Editor::CacheScene()
	{
		LUMOS_PROFILE_FUNCTION();
		m_Application->GetCurrentScene()->Serialise(m_TempSceneSaveFilePath, false);
	}
    
	void Editor::LoadCachedScene()
	{
		LUMOS_PROFILE_FUNCTION();
        
        if(FileSystem::FileExists(m_TempSceneSaveFilePath + m_Application->GetCurrentScene()->GetSceneName() + ".lsn"))
        {
            m_Application->GetCurrentScene()->Deserialise(m_TempSceneSaveFilePath, false);
        }
        else
        {
            std::string physicalPath;
            if(Lumos::VFS::Get()->ResolvePhysicalPath("/Scenes/" + m_Application->GetCurrentScene()->GetSceneName() + ".lsn", physicalPath))
            {
                auto newPath = StringUtilities::RemoveName(physicalPath);
                m_Application->GetCurrentScene()->Deserialise(newPath, false);
            }
        }
	}
    
}
