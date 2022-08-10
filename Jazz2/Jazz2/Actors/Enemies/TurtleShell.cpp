﻿#include "TurtleShell.h"
#include "../../LevelInitialization.h"
#include "../../ILevelHandler.h"
#include "../../Tiles/TileMap.h"
#include "../Solid/CrateContainer.h"
#include "../Weapons/ShotBase.h"
#include "../Weapons/FreezerShot.h"
#include "../Weapons/ToasterShot.h"

#include "../../../nCine/Base/Random.h"

namespace Jazz2::Actors::Enemies
{
	TurtleShell::TurtleShell()
		:
		_lastAngle(0.0f)
	{
	}

	void TurtleShell::Preload(const ActorActivationDetails& details)
	{
		uint8_t theme = details.Params[8];
		switch (theme) {
			case 0:
			default:
				PreloadMetadataAsync("Enemy/TurtleShell"_s);
				break;
			case 1: // Xmas
				PreloadMetadataAsync("Enemy/TurtleShellXmas"_s);
				break;
			case 2: // Tough (Boss)
				PreloadMetadataAsync("Boss/TurtleShellTough"_s);
				break;
		}
	}

	Task<bool> TurtleShell::OnActivatedAsync(const ActorActivationDetails& details)
	{
		SetHealthByDifficulty(1);
		CollisionFlags |= CollisionFlags::SkipPerPixelCollisions;

		_scoreValue = 100;

		_speed.X = *(float*)&details.Params[0];
		_externalForce.Y = *(float*)&details.Params[4];
		uint8_t theme = details.Params[8];
		switch (theme) {
			case 0:
			default:
				co_await RequestMetadataAsync("Enemy/TurtleShell"_s);
				break;
			case 1: // Xmas
				co_await RequestMetadataAsync("Enemy/TurtleShellXmas"_s);
				break;
			case 2: // Tough (Boss)
				co_await RequestMetadataAsync("Boss/TurtleShellTough"_s);
				break;
		}

		SetAnimation(AnimState::Idle);

		_canHurtPlayer = false;
		_friction = _levelHandler->Gravity * 0.05f;
		_elasticity = 0.5f;
		_health = 8;

		PlaySfx("Fly"_s);

		co_return true;
	}

	void TurtleShell::OnUpdate(float timeMult)
	{
		_speed.X = std::max(std::abs(_speed.X) - _friction, 0.0f) * (_speed.X < 0.0f ? -1.0f : 1.0f);

		double posYBefore = _pos.Y;

		TileCollisionParams params = { TileDestructType::Weapon | TileDestructType::Special, _speed.Y >= 0.0f, WeaponType::Blaster, 1 };
		TryStandardMovement(timeMult, params);
		OnUpdateHitbox();

		if (_renderer.AnimPaused) {
			if (_frozenTimeLeft <= 0.0f) {
				_renderer.AnimPaused = false;
				// TODO: Frozen effect
			} else {
				_frozenTimeLeft -= timeMult;
			}
		}

		HandleBlinking(timeMult);

		if (posYBefore - _pos.Y > 0.5 && std::abs(_speed.Y) < 1) {
			_speed.X = std::max(std::abs(_speed.X) - 10.0f * _friction, 0.0f) * (_speed.X < 0.0f ? -1.0f : 1.0f);
		}

		_lastAngle = lerp(_lastAngle, _speed.X * 0.06f, timeMult * 0.2f);
		_renderer.setRotation(_lastAngle);
	}

	void TurtleShell::OnUpdateHitbox()
	{
		UpdateHitbox(30, 16);
	}

	bool TurtleShell::OnPerish(ActorBase* collider)
	{
		CreateDeathDebris(collider);
		_levelHandler->PlayCommonSfx("Splat"_s, Vector3f(_pos.X, _pos.Y, 0.0f));

		TryGenerateRandomDrop();

		return EnemyBase::OnPerish(collider);
	}

	bool TurtleShell::OnHandleCollision(std::shared_ptr<ActorBase> other)
	{
		EnemyBase::OnHandleCollision(other);

		if (auto shotBase = dynamic_cast<Weapons::ShotBase*>(other.get())) {
			if (auto freezerShot = dynamic_cast<Weapons::FreezerShot*>(shotBase)) {
				return false;
			}

			if (auto toasterShot = dynamic_cast<Weapons::ToasterShot*>(shotBase)) {
				DecreaseHealth(INT32_MAX, toasterShot);
				return true;
			}

			float otherSpeed = other->GetSpeed().X;
			_speed.X = std::max(4.0f, std::abs(otherSpeed)) * (otherSpeed < 0.0f ? -0.5f : 0.5f);

			PlaySfx("Fly"_s);
		} else if (auto shell = dynamic_cast<TurtleShell*>(other.get())) {
			auto otherSpeed = shell->GetSpeed();
			if (std::abs(otherSpeed.Y - _speed.Y) > 1.0f && otherSpeed.Y > 0.0f) {
				DecreaseHealth(10, this);
			} else if (std::abs(_speed.X) > std::abs(otherSpeed.X)) {
				// Handle this only in the faster of the two
				//pos.X = collider.Transform.Pos.X + (speedX >= 0 ? -1f : 1f) * (currentAnimation.Base.FrameDimensions.X + 1);
				float totalSpeed = std::abs(_speed.X) + std::abs(otherSpeed.X);

				shell->_speed.X = totalSpeed / 2 * (_speed.X >= 0.0f ? 1.0f : -1.0f);
				_speed.X = totalSpeed / 2.0f * (_speed.X >= 0.0f ? -1.0f : 1.0f);

				shell->DecreaseHealth(1, this);
				PlaySfx("ImpactShell"_s, 0.8f);
				return true;
			}
		} else if (auto enemyBase = dynamic_cast<EnemyBase*>(other.get())) {
			if (enemyBase->CanCollideWithAmmo) {
				_speed.X = std::max(std::abs(_speed.X), 2.0f) * (_speed.X >= 0.0f ? -1.0f : 1.0f);
				if (!enemyBase->GetState(ActorFlags::IsInvulnerable)) {
					enemyBase->DecreaseHealth(1, this);
					return true;
				}
			}
		} else if (auto crateContainer = dynamic_cast<Solid::CrateContainer*>(other.get())) {
			_speed.X = std::max(std::abs(_speed.X), 2.0f) * (_speed.X >= 0.0f ? -1.0f : 1.0f);
			crateContainer->DecreaseHealth(1, this);
			return true;
		}

		return false;
	}

	void TurtleShell::OnHitFloor()
	{
		if (std::abs(_speed.Y) > 1.0f) {
			PlaySfx("ImpactGround"_s);
		}
	}
}