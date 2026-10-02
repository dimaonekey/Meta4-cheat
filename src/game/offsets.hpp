#pragma once

// ============================================================
//  OFFSETS — Standoff2 arm64-v8a
//  Все оффсеты строго здесь. Меняй только этот файл.
// ============================================================

// --- Static type offset (get_static) --- maybe type info
#define OFF_PLAYER_MANAGER          178356728

// --- PlayerManager fields ---
#define OFF_PM_LOCAL_PLAYER         0x70
#define OFF_PM_PLAYER_LIST          0x28

// --- PlayerList fields ---
#define OFF_LIST_COUNT              0x20
#define OFF_LIST_BUFFER             0x18
#define OFF_LIST_ENTRY_BASE         0x30
#define OFF_LIST_ENTRY_STRIDE       0x18

// --- Player fields ---
#define OFF_PLAYER_TEAM             0x79
#define OFF_PLAYER_MOVEMENT_CTRL    0x98
#define OFF_PLAYER_PHOTON_PTR       0x160
#define OFF_PLAYER_MAIN_CAMERA      0xE8

// --- MovementController fields ---
#define OFF_MC_TRANSFORM_DATA       0xB0

// --- TransformData fields ---
#define OFF_TD_POSITION             0x44

// --- PhotonPlayer fields ---
#define OFF_PHOTON_NAME             0x20
#define OFF_PHOTON_PROPS_REG        0x38

// --- PropertiesRegistry fields ---
#define OFF_PROPS_COUNT             0x20
#define OFF_PROPS_LIST              0x18
#define OFF_PROPS_KEY_BASE          0x28
#define OFF_PROPS_VAL_BASE          0x30
#define OFF_PROPS_VALUE_DATA        0x10

// --- Camera fields ---
#define OFF_CAM_TRANSFORM           0x20
#define OFF_CAM_TRANSFORM_MATRIX    0x10
#define OFF_CAM_MATRIX_DATA         0xF0

// --- Camera FOV ---
// lp → PlayerCamera(0xE8) → CameraSettings(0x28) → fieldOfView(0x38)
#define OFF_CAM_SETTINGS            0x28   // PlayerCamera → CameraSettings*
#define OFF_CAM_FOV                 0x38   // CameraSettings → fieldOfView (float)

// --- AimAnimController (viewmodel / hand position) ---
// lp → AimAnimController(0xA0) → viewmodelOffset(0xE8) vec3
// ВНИМАНИЕ: 0xA0 от lp — это AimAnimController, НЕ WeaponRootController
// OFF_PLAYER_WEAPON_ROOT тоже 0x88, от lp — разные поля, не конфликт
#define OFF_PLAYER_AIM_ANIM_CTRL    0xA0   // Player → AimAnimController*
#define OFF_AAC_VIEWMODEL_OFFSET    0xE8   // AimAnimController → viewmodelOffset (vec3, x/y/z)

// --- ESP ---
// Высота игрока (голова над позицией feet), метры
#define PLAYER_HEIGHT               1.67f

// --- il2cpp internals (get_static internals, НЕ ТРОГАТЬ) ---
// cls + 0x60  -> klass->static_fields (Il2CppClass)
// obj + 0x10  -> fields ptr
// Эти значения стабильны для il2cpp 27+, менять только при смене движка
// ── Combat: WeaponRootController (lp + OFF_PLAYER_WEAPON_ROOT) ──────────────
#define OFF_PLAYER_WEAPON_ROOT      0x88   // Player → WeaponRootController*
#define OFF_WRC_ACTIVE_WEAPON       0xA0   // WeaponRootController → WeaponController*

// ── Combat: WeaponController ─────────────────────────────────────────────────
#define OFF_WC_FIRE_DURATION        0x108  // SafeFloat: key@+0, enc@+4
#define OFF_WC_AMMO_CURRENT         0x120  // SafeBool pair: key@+0, val@+4
#define OFF_WC_AMMO_MAX             0x128  // SafeBool pair: key@+0, val@+4
#define OFF_WC_RECOIL_CTRL          0x160  // WeaponController → RecoilController*
#define OFF_WC_RECOIL_MULT          0x240  // plain float
#define OFF_WC_RECOIL_MULT2         0x244  // plain float
#define OFF_WC_WEAPON_PROPS         0xA8   // WeaponController → WeaponProperties*

// ── Combat: WeaponProperties ─────────────────────────────────────────────────
#define OFF_WP_WEAPON_ID            0x18   // int — 1..69 = gun
#define OFF_WP_PENETRATION          0x264  // float — wallshot: write 0x7F7FFFFF

// ── Combat: RecoilController ─────────────────────────────────────────────────
#define OFF_RC_RECOIL_0             0x18
#define OFF_RC_RECOIL_1             0x1C
#define OFF_RC_RECOIL_2             0x20
#define OFF_RC_RECOIL_3             0x24
#define OFF_RC_RECOIL_4             0x28
#define OFF_RC_RECOIL_5             0x2C

// ── Combat: MovementController → sub-controllers ─────────────────────────────
#define OFF_MC_TRAJECTORY           0xA8   // MovementController → TrajectoryPredictor*
#define OFF_MC_THRUST_DATA          0xB0   // MovementController → ThrustData*

// ── Combat: TrajectoryPredictor ──────────────────────────────────────────────
#define OFF_TP_JUMP_PARAMS          0x50   // TrajectoryPredictor → JumpParams*
#define OFF_TP_CROUCH_PARAMS        0x48   // TrajectoryPredictor → CrouchParams*

// ── Combat: JumpParams ───────────────────────────────────────────────────────
#define OFF_JP_JUMP_SPEED           0x10   // float — bunnyhop
#define OFF_JP_JUMP_SPEED2          0x60   // float — bunnyhop + airstrafe

// ── Combat: CrouchParams ─────────────────────────────────────────────────────
#define OFF_CP_SPEED_MULT           0x10   // float
#define OFF_CP_SPEED_MULT2          0x14   // float

// ── Combat: ThrustData ───────────────────────────────────────────────────────
#define OFF_TD_THRUST_VEC           0x68   // vec3 — airstrafe: обнуляем

// ── Aimbot: Player Aim Controller ─────────────────────────────────────────────
#define OFF_PLAYER_AIM_CONTROLLER   0x80   // Player → AimController*
#define OFF_AIM_CONTROLLER_DATA     0x90   // AimController → AimingData*

// ── Aimbot: AimingData ───────────────────────────────────────────────────────
// offsets0382 (2).cs: AimingData fields: Vector3 @ 0x18 and Vector3 @ 0x24
#define OFF_AIMDATA_PITCH           0x18   // float — pitch angle
#define OFF_AIMDATA_YAW             0x1C   // float — yaw angle
#define OFF_AIMDATA_PITCH_YAW_VEC3  0x18   // vec3 — pitch/yaw/roll
#define OFF_AIMDATA_SECOND_VEC3     0x24   // vec3 — secondary angle data

// ── Aimbot: Camera System ─────────────────────────────────────────────────────
#define OFF_PLAYER_MAIN_CAM_DATA    0x40   // PlayerMainCamera → CameraData*
#define OFF_CAM_MOVEMENT_CTRL       0xA0   // CameraData → MovementController*
#define OFF_CAM_NATIVE              0x10   // MovementController → NativeCamera*
#define OFF_NATIVE_CAM_ASPECT       0x4F0  // NativeCamera → aspectRatio (float)
#define OFF_NATIVE_CAM_FOV_FIELD    0x180  // NativeCamera → fieldOfView (float)

// ── Player Visibility & Occlusion ─────────────────────────────────────────────
#define OFF_PLAYER_OCCLUSION        0xB8   // Player → OcclusionController*
#define OFF_OCCLUSION_CURRENT       0x34   // OcclusionController → currentState (int)
#define OFF_OCCLUSION_NEXT          0x38   // OcclusionController → nextState (int)

// ── Player Bones & Skeleton ───────────────────────────────────────────────────
#define OFF_PLAYER_VIEW_1           0x48   // Player → View1*
#define OFF_PLAYER_VIEW_2           0x50   // Player → View2*
#define OFF_VIEW_BIPED_MAP          0x48   // View → BipedMap*

// ── Bone Offsets (from BipedMap) ───────────────────────────────────────────────
#define OFF_BONE_HEAD_1             0x28   // BipedMap → HeadBone1*
#define OFF_BONE_HEAD_2             0x20   // BipedMap → HeadBone2*
#define OFF_BONE_CHEST_1            0x40   // BipedMap → ChestBone1*
#define OFF_BONE_CHEST_2            0x38   // BipedMap → ChestBone2*
#define OFF_BONE_CHEST_3            0x30   // BipedMap → ChestBone3*
#define OFF_BONE_CHEST_4            0x88   // BipedMap → ChestBone4*
#define OFF_BONE_FOOT_LEFT          0x98   // BipedMap → LeftFoot*
#define OFF_BONE_FOOT_RIGHT         0xB8   // BipedMap → RightFoot*
#define OFF_BONE_SPINE_1            0x88   // BipedMap → SpineBone1*
#define OFF_BONE_SPINE_2            0x90   // BipedMap → SpineBone2*
#define OFF_BONE_SPINE_3            0xB0   // BipedMap → SpineBone3*

// ── Transform System ───────────────────────────────────────────────────────────
#define OFF_TRANSFORM_NATIVE        0x10   // Transform → NativeTransform*
#define OFF_NATIVE_TRANSFORM_DATA   0x38   // NativeTransform → TransformData*
#define OFF_NATIVE_TRANSFORM_INDEX  0x40   // NativeTransform → index (int)
#define OFF_NATIVE_TRANSFORM_DIRECT 0x90   // NativeTransform → directPosition (vec3)
#define OFF_TRANSFORM_DATA_ARRAY    0x18   // TransformData → transformArray*
#define OFF_TRANSFORM_DATA_INDICES  0x20   // TransformData → indicesArray*

// ── Aimbot Constants ───────────────────────────────────────────────────────────
#define AIMBOT_VERTICAL_FOV_DEG     70.0f  // Vertical FOV in degrees
#define AIMBOT_PI                   3.14159265f
#define AIMBOT_RAD_TO_DEG           57.2957795f

// ── Alternative Player Manager Offsets (fallback) ──────────────────────────────
#define OFF_PLAYER_MANAGER_ALT      132435632ULL  // Alternative PM offset


// --- Armor (ЗАГЛУШКА — подставь реальный оффсет когда найдёшь) ---
// Ожидаемый тип: int (0-100)
#define OFF_PLAYER_ARMOR            0x000  // TODO: реальный оффсет брони

// ── Triggerbot (перенесено из Welo) ──────────────────────────────────────────
#define OFF_WC_FIRE_BUTTON          0x148  // WeaponController → fireButton (uint8_t)
#define TRIGGER_SHOT_DURATION       0.08f  // длительность нажатия кнопки огня
#define TRIGGER_MAX_RADIUS_PX       80.f
#define TRIGGER_MIN_RADIUS_PX       12.f
#define TRIGGER_REF_DIST_M          100.f  // базовая дистанция для адаптивного радиуса

// ── Bones / get_bones (перенесено из Welo) ───────────────────────────────────
#define OFF_BIPED_MAP_BONE_COUNT    22     // кол-во костей в bones_t
#define OFF_BIPED_MAP_PTRS_BASE     0x20   // BipedMap → bone transform ptr array
#define BONE_MATRIX_DATA            0x28   // NativeTransform → TransformData*  (внутри get_bone_pos)
#define BONE_TRANSFORM_IDX          0x30   // NativeTransform → index
#define BONE_MATRIX_LIST            0x18   // TransformData → transformArray*
#define BONE_MATRIX_INDICES         0x20   // TransformData → indicesArray*
#define OFF_TP_JUMP_PARAMS          0x50   // TrajectoryPredictor → JumpParams*
#define OFF_TP_CROUCH_PARAMS        0x48   // TrajectoryPredictor → CrouchParams*

// ── Combat: JumpParams ───────────────────────────────────────────────────────
#define OFF_JP_JUMP_SPEED           0x10   // float — bunnyhop
#define OFF_JP_JUMP_SPEED2          0x60   // float — bunnyhop + airstrafe
#define OFF_JP_JUMP_FORCE           0x10   // float — high jump force (same slot, different value range)

// ── Combat: CrouchParams ─────────────────────────────────────────────────────
#define OFF_CP_SPEED_MULT           0x10   // float
#define OFF_CP_SPEED_MULT2          0x14 
