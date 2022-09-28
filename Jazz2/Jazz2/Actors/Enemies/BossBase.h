﻿#pragma once

#include "EnemyBase.h"

namespace Jazz2::Actors::Enemies
{
	class BossBase : public EnemyBase
	{
	public:
		virtual bool OnActivatedBoss() = 0;
		virtual void OnDeactivatedBoss() { };

		bool OnPlayerDied();

	protected:
		bool OnTileDeactivated() override;
	};
}