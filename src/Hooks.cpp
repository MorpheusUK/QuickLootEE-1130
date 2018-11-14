#include "Hooks.h"

#include "skse64/GameReferences.h"  // g_thePlayer
#include "skse64/GameTypes.h"  // BSString
#include "skse64_common/BranchTrampoline.h"  // g_branchTrampoline
#include "skse64_common/SafeWrite.h"  // SafeWrite64
#include "xbyak/xbyak.h"

#include <string>  // string
#include <sstream>  // stringstream

#include "HaACTITextOverrideVisitor.h"  // HaACTITextOverrideVisitor
#include "LootMenu.h"  // LootMenu
#include "Offsets.h"

#include "RE/ActivateHandler.h"  // ActivateHandler
#include "RE/BSWin32GamepadDevice.h"  // BSWin32GamepadDevice
#include "RE/ButtonEvent.h"  // ButtonEvent
#include "RE/FavoritesHandler.h"  // FavoritesHandler
#include "RE/MenuManager.h"  // MenuManager
#include "RE/PlayerCharacter.h"  // PlayerCharacter
#include "RE/PlayerControls.h"  // PlayerControls, PlayerControls::Data024
#include "RE/PlayerInputHandler.h"  // PlayerInputHandler
#include "RE/ReadyWeaponHandler.h"  // ReadyWeaponHandler
#include "RE/TESBoundAnimObject.h"  // TESObjectACTI
#include "RE/TESObjectREFR.h"  // TESObjectREFR

class PlayerCharacter;


namespace Hooks
{
	RelocAddr<_SendItemsPickPocketedEvent_t*> _SendItemsPickPocketedEvent(SEND_ITEMS_PICK_POCKETED_EVENT);
	RelocAddr<_GetPickPocketChance_t*> _GetPickPocketChance(GET_PICK_POCKET_CHANCE);


	template <uintptr_t offset>
	class CameraStateHandlerEx : public RE::PlayerInputHandler
	{
	public:
		typedef bool(CameraStateHandlerEx::*_CanProcess_t)(InputEvent* a_event);
		static _CanProcess_t orig_CanProcess;


		bool hook_CanProcess(InputEvent* a_event)
		{
			static InputStringHolder* strHolder = InputStringHolder::GetSingleton();

			bool result = (this->*orig_CanProcess)(a_event);
			if (a_event && result && QuickLootRE::LootMenu::IsVisible()) {
				result = (*a_event->GetControlID() != strHolder->togglePOV);
			}

			return result;
		}


		static void installHook()
		{
			RelocPtr<_CanProcess_t> vtbl_CanProcess(offset);
			orig_CanProcess = *vtbl_CanProcess;
			SafeWrite64(vtbl_CanProcess.GetUIntPtr(), GetFnAddr(&hook_CanProcess));
		}
	};


	template <uintptr_t offset> typename CameraStateHandlerEx<offset>::_CanProcess_t CameraStateHandlerEx<offset>::orig_CanProcess;
	typedef CameraStateHandlerEx<FIRST_PERSON_STATE_VTBL_META + 0x60> FirstPersonStateHandler;
	typedef CameraStateHandlerEx<THIRD_PERSON_STATE_VTBL_META + 0x98> ThirdPersonStateHandler;


	class FavoritesHandlerEx : public RE::FavoritesHandler
	{
	public:
		typedef bool(FavoritesHandlerEx::*_CanProcess_t)(InputEvent* a_event);
		static _CanProcess_t orig_CanProcess;


		bool hook_CanProcess(InputEvent* a_event)
		{
			using QuickLootRE::LootMenu;
			typedef RE::BSWin32GamepadDevice::Gamepad Gamepad;

			bool result = (this->*orig_CanProcess)(a_event);
			if (a_event && result && LootMenu::IsVisible()) {
				if (a_event->deviceType == kDeviceType_Gamepad && a_event->eventType == InputEvent::kEventType_Button) {
					ButtonEvent* button = static_cast<ButtonEvent*>(a_event);
					result = (button->keyMask != Gamepad::kGamepad_Up && button->keyMask != Gamepad::kGamepad_Down);
				}
			}

			return result;
		}


		static void installHook()
		{
			RelocPtr<_CanProcess_t> vtbl_CanProcess(FAVORITES_HANDLER_VTBL_META + 0x10);
			orig_CanProcess = *vtbl_CanProcess;
			SafeWrite64(vtbl_CanProcess.GetUIntPtr(), GetFnAddr(&hook_CanProcess));
		}
	};


	FavoritesHandlerEx::_CanProcess_t FavoritesHandlerEx::orig_CanProcess;


	class ReadyWeaponHandlerEx : public RE::ReadyWeaponHandler
	{
	public:
		typedef void(ReadyWeaponHandlerEx::*_ProcessButton_t)(RE::ButtonEvent* a_event, RE::PlayerControls::Data024* a_data);
		static _ProcessButton_t orig_ProcessButton;


		void hook_ProcessButton(RE::ButtonEvent* a_event, RE::PlayerControls::Data024* a_data)
		{
			using QuickLootRE::LootMenu;

			static RE::PlayerCharacter*	player		= reinterpret_cast<RE::PlayerCharacter*>(*g_thePlayer);
			static UIManager*			uiManager	= UIManager::GetSingleton();
			static UIStringHolder*		strHolder	= UIStringHolder::GetSingleton();
			static RE::MenuManager*		mm			= RE::MenuManager::GetSingleton();

			if (a_event && LootMenu::IsVisible()) {
				if (a_event->IsDown()) {  // This MUST be on down
					player->StartActivation();
				}
			} else {
				(this->*orig_ProcessButton)(a_event, a_data);
			}
		}


		static void installHook()
		{
			RelocPtr<_ProcessButton_t> vtbl_ProcessButton(READY_WEAPON_HANDLER_VTBL_META + 0x28);
			orig_ProcessButton = *vtbl_ProcessButton;
			SafeWrite64(vtbl_ProcessButton.GetUIntPtr(), GetFnAddr(&hook_ProcessButton));
		}
	};


	ReadyWeaponHandlerEx::_ProcessButton_t ReadyWeaponHandlerEx::orig_ProcessButton;


	class ActivateHandlerEx : public RE::ActivateHandler
	{
	public:
		typedef bool(ActivateHandlerEx::*_CanProcess_t)(InputEvent* a_event);
		static _CanProcess_t orig_CanProcess;


		bool hook_CanProcess(InputEvent* a_event)
		{
			using QuickLootRE::LootMenu;

			static RE::PlayerCharacter*	player		= reinterpret_cast<RE::PlayerCharacter*>(*g_thePlayer);
			static InputStringHolder*	strHolder	= InputStringHolder::GetSingleton();

			if (player->GetGrabbedRef()) {
				LootMenu::Close();
			}

			BSFixedString str = *a_event->GetControlID();
			if (LootMenu::IsVisible() && str == strHolder->activate && (a_event->eventType == InputEvent::kEventType_Button)) {
				RE::ButtonEvent* button = static_cast<RE::ButtonEvent*>(a_event);
				if (button->IsUp()) {
					LootMenu::GetSingleton()->TakeItemStack();
					return false;
				} else if (button->IsDown()) {  // inventory menu activation will queue up without this
					return false;
				}
			}
			return (this->*orig_CanProcess)(a_event);
		}


		static void installHook()
		{
			RelocPtr<_CanProcess_t> vtbl_CanProcess(ACTIVATE_HANDLER_VTBL_META + 0x10);
			orig_CanProcess = *vtbl_CanProcess;
			SafeWrite64(vtbl_CanProcess.GetUIntPtr(), GetFnAddr(&hook_CanProcess));
		}
	};


	ActivateHandlerEx::_CanProcess_t ActivateHandlerEx::orig_CanProcess;


	template <uintptr_t offset>
	class TESBoundAnimObjectEx : public RE::TESBoundAnimObject
	{
	public:
		typedef bool(TESBoundAnimObjectEx::*_GetCrosshairText_t)(RE::TESObjectREFR* a_ref, BSString* a_dst, bool a_unk);
		static _GetCrosshairText_t orig_GetCrosshairText;


		bool hook_GetCrosshairText(RE::TESObjectREFR* a_ref, BSString* a_dst, bool a_unk)
		{
			typedef RE::BGSEntryPointPerkEntry::EntryPointType EntryPointType;
			using QuickLootRE::LootMenu;
			using QuickLootRE::HaACTITextOverrideVisitor;
			static RE::PlayerCharacter* player = reinterpret_cast<RE::PlayerCharacter*>(*g_thePlayer);

			bool result = (this->*orig_GetCrosshairText)(a_ref, a_dst, a_unk);

			if (LootMenu::CanOpen(a_ref, player->IsSneaking())) {
				std::stringstream ss(a_dst->Get());
				std::string dispText;
				if (std::getline(ss, dispText, '\n')) {
					if (dispText[0] == '<') {
						int beg = dispText.find_first_of('>');
						int end = dispText.find_last_of('<');
						if (beg != std::string::npos && end != std::string::npos) {
							std::string subStr = dispText.substr(beg + 1, end - beg - 1);
							LootMenu::SetActiText(subStr.c_str());
						}
					} else {
						LootMenu::SetActiText(dispText.c_str());
					}
				}

#if 0
				if (player->CanProcessEntryPointPerkEntry(EntryPointType::kEntryPoint_Set_Activate_Label)) {
					HaACTITextOverrideVisitor visitor(player, a_ref);
					player->VisitEntryPointPerkEntries(EntryPointType::kEntryPoint_Set_Activate_Label, visitor);
				}
#endif

				return false;

			} else {
				return result;
			}
		}


		static void installHook()
		{
			RelocPtr<_GetCrosshairText_t> vtbl_GetCrosshairText(offset);
			orig_GetCrosshairText = *vtbl_GetCrosshairText;
			SafeWrite64(vtbl_GetCrosshairText.GetUIntPtr(), GetFnAddr(&hook_GetCrosshairText));
		}
	};


	template <uintptr_t offset> typename TESBoundAnimObjectEx<offset>::_GetCrosshairText_t TESBoundAnimObjectEx<offset>::orig_GetCrosshairText;
	typedef TESBoundAnimObjectEx<TES_OBJECT_ACTI_VTBL_META + 0x268> TESObjectACTIEx;
	typedef TESBoundAnimObjectEx<TES_OBJECT_CONT_VTBL_META + 0x268> TESObjectCONTEx;
	typedef TESBoundAnimObjectEx<TES_NPC_VTBL_META + 0x268> TESNPCEx;


	void installHooks()
	{
		FirstPersonStateHandler::installHook();
		ThirdPersonStateHandler::installHook();
		FavoritesHandlerEx::installHook();
		ReadyWeaponHandlerEx::installHook();
		ActivateHandlerEx::installHook();
		TESObjectACTIEx::installHook();
		TESObjectCONTEx::installHook();
		TESNPCEx::installHook();
	}
}
