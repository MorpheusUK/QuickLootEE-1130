#include "Hooks.h"

#include "skse64/GameReferences.h"  // g_thePlayer
#include "skse64_common/BranchTrampoline.h"  // g_branchTrampoline
#include "skse64_common/SafeWrite.h"  // SafeWrite64

#include "LootMenu.h"  // LootMenu
#include "Offsets.h"

#include "RE/PlayerControls.h"  // PlayerControls, PlayerControls::Data024


class PlayerCharacter;


namespace Hooks
{
	RelocAddr<_StartActivation_Fn*> StartActivation_Fn(START_ACTIVATION_FN);
	RelocPtr<uintptr_t> StartActivation_Call(START_ACTIVATION_CALL);

	RelocPtr<_ReadyWeaponHandler_ProcessButton> vtbl_ReadyWeaponHandler_ProcessButton(MOVEMENT_HANDLER_VTBL_META + (0x1B * 0x8));
	_ReadyWeaponHandler_ProcessButton orig_ReadyWeaponHandler_ProcessButton;


	void hook_StartActivation_Call(PlayerCharacter* a_player)
	{
		using QuickLootRE::LootMenu;
		LootMenu* singleton;
		if (singleton = LootMenu::GetSingleton()) {
			singleton->TakeItem();
		} else {
			StartActivation_Fn(a_player);
		}
	}


	void hook_ReadyWeaponHandler_ProcessButton(RE::PlayerControls* a_handler, ButtonEvent* a_event, RE::PlayerControls::Data024* a_data)
	{
		using QuickLootRE::LootMenu;
		if (LootMenu::GetSingleton()) {
			CALL_MEMBER_FN(UIManager::GetSingleton(), AddMessage)(&LootMenu::GetName(), UIMessage::kMessage_Close, 0);
			LootMenu::ClearContainerRef();
			StartActivation_Fn((*g_thePlayer));
		} else {
			orig_ReadyWeaponHandler_ProcessButton(a_handler, a_event, a_data);
		}
	}


	void installHooks()
	{
		g_branchTrampoline.Write5Call(StartActivation_Call.GetUIntPtr(), GetFnAddr(hook_StartActivation_Call));

		orig_ReadyWeaponHandler_ProcessButton = *vtbl_ReadyWeaponHandler_ProcessButton;
		SafeWrite64(vtbl_ReadyWeaponHandler_ProcessButton.GetUIntPtr(), GetFnAddr(hook_ReadyWeaponHandler_ProcessButton));
	}
}
