#pragma once
// Runtime-mutable offset table.
//
// The values below are the build-time fallback baked in from the last synced
// cheatoffsets dump. src/engine/offsets_auto.h overwrites them at boot from the
// on-disk cache and again on every successful poll of the feed, so a game patch
// no longer requires a rebuild.
//
// Do not #define these — the updater needs to write them at runtime.
//
// Values marked "[sdk]" are cross-checked against the local SDK dump
// (classes.txt / structs.txt) for the current build.
//
// Synced: ++Fortnite+Release-42.40-CL-59343437 (hotfix, 2026-10-21) against the
// organized ext-project dump. "[rt]" marks runtime-resolved values, "[live]"
// marks bitfields where the SDK dump's bit position was wrong and the value
// below is the live-verified one.

#include <cstdint>

namespace off {

    // ---- engine / world chain -------------------------------------------
    inline std::uint64_t GENGINE            = 0x1B098778; // [rt] kGEngineRVA, CL-59343437
    inline std::uint64_t VIEWPORT           = 0xB70;      // UEngine -> GameViewport
    inline std::uint64_t VIEWPORT_WORLD     = 0x78;       // UGameViewportClient -> World
    inline std::uint64_t GAME_INSTANCE      = 0x248;      // UWorld -> OwningGameInstance
    inline std::uint64_t GAME_STATE         = 0x1D0;      // UWorld -> GameState
    inline std::uint64_t LOCAL_PLAYERS      = 0x38;       // UGameInstance -> LocalPlayers
    inline std::uint64_t PLAYER_CONTROLLER  = 0x30;       // UPlayer -> PlayerController
    inline std::uint64_t PLAYER_STATE       = 0x278;      // AController -> PlayerState
    inline std::uint64_t LOCAL_PAWN         = 0x318;      // APlayerController -> AcknowledgedPawn
    inline std::uint64_t ROTATION_INPUT     = 0x4B0;      // APlayerController -> RotationInput (FRotator, 3 doubles; aim write target)
    inline std::uint64_t PLAYER_ARRAY       = 0x288;      // AGameStateBase -> PlayerArray
    inline std::uint64_t NETCONNECTION      = 0x4A8;      // APlayerController -> NetConnection

    // ---- player state ----------------------------------------------------
    inline std::uint64_t PAWN_PRIVATE       = 0x2E8;      // APlayerState -> PawnPrivate
    inline std::uint64_t TEAM_INDEX         = 0xF69;      // AFortPlayerStateAthena -> TeamIndex
    inline std::uint64_t KILL_SCORE         = 0xF80;      // AFortPlayerStateAthena -> KillScore (uint32)
    inline std::uint64_t HABANERO_COMPONENT = 0x918;      // AFortPlayerState -> HabaneroComponent
    inline std::uint64_t RANKED_PROGRESS    = 0xD8;       // UFortPlayerStateComponent_Habanero -> RankedProgress
    inline std::uint64_t PLATFORM           = 0x400;      // AFortPlayerState -> Platform
    inline std::uint64_t PLAYERNAME         = 0x308;      // APlayerState -> PlayerNamePrivate

    // ---- pawn / weapon ---------------------------------------------------
    inline std::uint64_t MESH               = 0x2F0;      // ACharacter -> Mesh
    inline std::uint64_t CURRENT_WEAPON     = 0x9D0;      // AFortPawn -> CurrentWeapon
    inline std::uint64_t HEALTH_SET         = 0x13D8;     // AFortPawn -> HealthSet (UFortHealthSet*)
    inline std::uint64_t WEAPON_DATA        = 0x610;      // [sdk 42.40] AFortWeapon -> WeaponData (0x648 was HitNotifyAudioBank)
    inline std::uint64_t WEAPON_NAME        = 0x38;       // UItemDefinitionBase -> ItemName
    inline std::uint64_t AMMO_COUNT         = 0x10D8;     // [sdk 42.40] AFortWeapon -> AmmoCount (SDK + feed agree; 0x11B0 was stale)
    inline std::uint64_t IS_RELOADING       = 0x379;      // AFortWeapon -> bIsReloadingWeapon (bit 0)
    inline std::uint64_t IS_RELOADING_MASK  = 0x01;

    // ---- components ------------------------------------------------------
    // [sdk 42.40] USkeletalMeshComponent::CachedComponentSpaceTransforms. the
    // ext-project dump carried 0xA00 here, which is actually CachedBoneSpace-
    // Transforms — Fortnite nulls that array on remote pawns, so every enemy
    // resolved bones=0x0 and the ESP dropped the actor (sdk_diag.log 2026-10-24).
    inline std::uint64_t BONE_ARRAY         = 0xA10;
    // CachedBoneSpaceTransforms — usually null on remote pawns; kept as a
    // fallback for the local mesh. the old 0x9F0 read LinkedInstances padding.
    inline std::uint64_t BONE_ARRAY_ALT     = 0xA00;
    inline std::uint64_t ROOT_COMPONENT     = 0x1B0;      // AActor -> RootComponent
    inline std::uint64_t RELATIVE_LOCATION  = 0x140;      // USceneComponent -> RelativeLocation
    inline std::uint64_t COMPONENT_TO_WORLD = 0x1E0;      // USceneComponent -> ComponentToWorld
    inline std::uint64_t LAST_RENDER_TIME   = 0x548;      // [rt] UPrimitiveComponent -> LastRenderTime (float)
    inline std::uint64_t CAPSULE_COMPONENT  = 0x300;      // ACharacter -> CapsuleComponent
    inline std::uint64_t CAPSULE_HALF_HEIGHT= 0x590;      // UCapsuleComponent -> CapsuleHalfHeight (float)
    inline std::uint64_t CAPSULE_RADIUS     = 0x594;      // UCapsuleComponent -> CapsuleRadius (float)

    // ---- health / shield (UFortHealthSet via HEALTH_SET) -----------------
    // Each attribute is an FGameplayAttributeData whose live value sits at
    // +0xC (CurrentValue); the +0xC is already baked into these offsets, so a
    // direct read<float> at health_set + HEALTH returns the live number.
    inline std::uint64_t HEALTH             = 0x54;       // 0x48 + 0xC
    inline std::uint64_t SHIELD             = 0xA4;       // 0x98 + 0xC
    inline std::uint64_t CURRENT_SHIELD     = 0xCC;       // 0xC0 + 0xC (fallback)

    // ---- pawn status bitfields (read: read<uint8_t>(pawn + OFF) & MASK) --
    inline std::uint64_t IS_BOT             = 0x27A;      // APlayerState -> bIsABot, bit 3
    inline std::uint64_t IS_BOT_MASK        = 0x08;
    inline std::uint64_t IS_CROUCHED        = 0x430;      // ACharacter -> bIsCrouched, bit 1
    inline std::uint64_t IS_CROUCHED_MASK   = 0x02;
    inline std::uint64_t IS_DYING           = 0x728;      // [live] AFortPawn -> bIsDying, bit 5 (dump bit 3 is wrong)
    inline std::uint64_t IS_DYING_MASK      = 0x20;
    inline std::uint64_t IS_DBNO            = 0x881;      // [live] AFortPawn -> bIsDBNO, bit 7 (dump bit 6 is wrong)
    inline std::uint64_t IS_DBNO_MASK       = 0x80;
    inline std::uint64_t IS_SLIDING         = 0x758;      // AFortPawn -> bIsSliding, bit 6
    inline std::uint64_t IS_SLIDING_MASK    = 0x40;
    inline std::uint64_t IS_SKYDIVING       = 0x22BF;     // AFortPlayerPawn -> bIsSkydiving (plain bool, unaligned)

    // ---- camera ----------------------------------------------------------
    // [sdk] APlayerController -> APlayerCameraManager
    inline std::uint64_t PLAYER_CAMERA_MANAGER = 0x328;
    // [sdk] APlayerCameraManager -> FCameraCacheEntry CameraCachePrivate
    inline std::uint64_t CAMERA_CACHE          = 0x1590;
    // [sdk] LastFrameCameraCachePrivate — CameraCachePrivate can hold a stale or
    // zeroed entry during load/spectate transitions; the camera read falls back
    // to this before assuming a default FOV.
    inline std::uint64_t LAST_FRAME_CAMERA_CACHE = 0x1EB0;
    // [sdk] FCameraCacheEntry -> FMinimalViewInfo POV
    inline std::uint64_t CAMERA_POV            = 0x010;
    // [sdk] FMinimalViewInfo: Location (FVector), Rotation (FRotator), FOV (float)
    inline std::uint64_t POV_LOCATION          = 0x000;
    inline std::uint64_t POV_ROTATION          = 0x018;
    inline std::uint64_t POV_FOV               = 0x030;

    // ---- misc ------------------------------------------------------------
    inline std::uint64_t SECONDS            = 0x198;      // UWorld world clock
}

using off::GENGINE;
using off::VIEWPORT;
using off::VIEWPORT_WORLD;
using off::GAME_INSTANCE;
using off::GAME_STATE;
using off::LOCAL_PLAYERS;
using off::PLAYER_CONTROLLER;
using off::PLAYER_STATE;
using off::LOCAL_PAWN;
using off::ROTATION_INPUT;
using off::PLAYER_ARRAY;
using off::NETCONNECTION;
using off::PAWN_PRIVATE;
using off::TEAM_INDEX;
using off::KILL_SCORE;
using off::HABANERO_COMPONENT;
using off::RANKED_PROGRESS;
using off::PLATFORM;
using off::PLAYERNAME;
using off::MESH;
using off::CURRENT_WEAPON;
using off::HEALTH_SET;
using off::WEAPON_DATA;
using off::WEAPON_NAME;
using off::AMMO_COUNT;
using off::IS_RELOADING;
using off::IS_RELOADING_MASK;
using off::BONE_ARRAY;
using off::BONE_ARRAY_ALT;
using off::ROOT_COMPONENT;
using off::RELATIVE_LOCATION;
using off::COMPONENT_TO_WORLD;
using off::LAST_RENDER_TIME;
using off::CAPSULE_COMPONENT;
using off::CAPSULE_HALF_HEIGHT;
using off::CAPSULE_RADIUS;
using off::HEALTH;
using off::SHIELD;
using off::CURRENT_SHIELD;
using off::IS_BOT;
using off::IS_BOT_MASK;
using off::IS_CROUCHED;
using off::IS_CROUCHED_MASK;
using off::IS_DYING;
using off::IS_DYING_MASK;
using off::IS_DBNO;
using off::IS_DBNO_MASK;
using off::IS_SLIDING;
using off::IS_SLIDING_MASK;
using off::IS_SKYDIVING;
using off::PLAYER_CAMERA_MANAGER;
using off::CAMERA_CACHE;
using off::LAST_FRAME_CAMERA_CACHE;
using off::CAMERA_POV;
using off::POV_LOCATION;
using off::POV_ROTATION;
using off::POV_FOV;
using off::SECONDS;
