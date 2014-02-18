/*
* Copyright (C) 2005 - 2011 MaNGOS <http://www.getmangos.org/>
*
* Copyright (C) 2008 - 2011 TrinityCore <http://www.trinitycore.org/>
*
* Copyright (C) 2011 TrilliumEMU <http://www.arkania.net/>
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by the
* Free Software Foundation; either version 2 of the License, or (at your
* option) any later version.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
* more details.
*
* You should have received a copy of the GNU General Public License along
* with this program. If not, see <http://www.gnu.org/licenses/>.
*/

//script by Naios

#include "ScriptPCH.h"
#include "firelands.h"

enum Yells
{
	SAY_AGGRO                                    = -1999971,
	SAY_SOFT_ENRAGE								 = -1999972, //TODO Add Sound
	SAY_ON_DOGS_FALL							 = -1999973, //TODO Add Sound
	SAY_ON_DEAD									 = -1999974, //TODO Add Sound
};

enum Spells
{

	//Shannox
	SPELL_ARCTIC_SLASH_10N                      = 99931,
	SPELL_ARCTIC_SLASH_25N                      = 101201,
	SPELL_ARCTIC_SLASH_10H                      = 101202,
	SPELL_ARCTIC_SLASH_25H                      = 101203,

	SPELL_BERSERK                               = 26662,

	SPELL_CALL_SPEAR                            = 100663,
	SPELL_HURL_SPEAR                            = 100002,   // Dummy Effect & Damage
	SPELL_HURL_SPEAR_SUMMON                     = 99978,    //Summons Spear of Shannox
	SPELL_HURL_SPEAR_DUMMY_SCRIPT               = 100031,
	SPELL_MAGMA_RUPTURE_SHANNOX                 = 99840,

	SPELL_FRENZY_SHANNOX                        = 23537,
	SPELL_IMMOLATION_TRAP                       = 52606,

	// Riplimb
	SPELL_LIMB_RIP                              = 99832,
	SPELL_DOGGED_DETERMINATION                  = 101111,

	// Rageface
	SPELL_FACE_RAGE                             = 99947,

	// Both Dogs
	SPELL_FRENZIED_DEVOLUTION                   = 100064,
	SPELL_FEEDING_FRENZY_H                      = 100655,

	SPELL_WARY_10N                              = 100167, // Buff when the Dog gos in a Trap
	SPELL_WARY_25N                              = 101215,
	SPELL_WARY_10H                              = 101216,
	SPELL_WARY_25H                              = 101217,

	// Misc
	SPELL_SEPERATION_ANXIETY                    = 99835,

	//Spear Abilities
	SPELL_MAGMA_FLARE                           = 100495, // Inflicts Fire damage to enemies within 50 yards.
	SPELL_MAGMA_RUPTURE                         = 100003, // Calls forth magma eruptions to damage nearby foes. (Dummy Effect)
	SPELL_MAGMA_RUPTURE_VISUAL                  = 99841,

	//Traps Abilities
	CRYSTAL_PRISON_EFFECT                       = 99837,

};

enum Events
{
	//Shannox
	EVENT_IMMOLTATION_TRAP                      = 1, // Every 10s
	EVENT_BERSERK                               = 2, // After 10m
	EVENT_ARCING_SLASH                          = 3, // Every 12s
	EVENT_HURL_SPEAR_OR_MAGMA_RUPTUTRE          = 4, // Every 42s
	EVENT_SUMMON_SPEAR                          = 5, // After EVENT_HURL_SPEAR_OR_MAGMA_RUPTUTRE
	EVENT_SUMMON_CRYSTAL_PRISON                 = 6, // Every 25s

	//Riplimb
	EVENT_LIMB_RIP                              = 7, // i Dont know...
	EVENT_RIPLIMB_RESPAWN_H                     = 8,
	EVENT_TAKING_SPEAR_DELAY                    = 9,

	//Rageface
	EVENT_FACE_RAGE                             = 10, // Every 31s

	// Trigger for the Crystal Trap
	EVENT_CRYSTAL_TRAP_TRIGGER                  = 11,

	// Trigger for self Dispawn (Crystal Prison)
	EVENT_CRYSTAL_PRISON_DESPAWN                = 12,

};

Position const bucketListPositions[5] =  {{0.0f,0.0f,0.0f,0.0f},
										  {0.0f,0.0f,0.0f,0.0f},
										  {0.0f,0.0f,0.0f,0.0f},
										  {0.0f,0.0f,0.0f,0.0f},
										  {0.0f,0.0f,0.0f,0.0f}};

// Dogs Walking Distance to Shannox
const float walkRagefaceDistance = 8;
const float walkRagefaceAngle = 7;

const float walkRiplimbDistance = 9;
const float walkRiplimbAngle = 6;

// Damage needed that Rageface changes his Target
const int damageNeededThatRagefaceChangesTarget = 45000; // Blizzlike

// If the Distance between Shannox & Dogs > This Value, all 3 get the Seperation Buff
const float maxDistanceBetweenShannoxAndDogs = 70;



class boss_shannox : public CreatureScript
{
public:
	boss_shannox() : CreatureScript("boss_shannox"){}

	CreatureAI* GetAI(Creature* creature) const
	{
		return new boss_shannoxAI(creature);
	}

	struct boss_shannoxAI : public BossAI
	{
		boss_shannoxAI(Creature* c) : BossAI(c, DATA_SHANNOX)
		{
			instance = me->GetInstanceScript();
			// TODO Add not Tauntable Flag

			pRiplimb = NULL;
			pRageface = NULL;

			softEnrage = false;
			riplimbIsRespawning = false;

			Reset();
		}

		InstanceScript* instance;
		Creature* pRiplimb;
		Creature* pRageface;
		bool softEnrage;
		bool riplimbIsRespawning;
		bool bucketListCheckPoints[5];
		bool hurlSpeer;
				
		void DespawnCreatures(uint32 entry, float distance)
		{
			std::list<Creature*> creatures;
			GetCreatureListWithEntryInGrid(creatures, me, entry, distance);

			if (creatures.empty())
				return;

			for (std::list<Creature*>::iterator iter = creatures.begin(); iter != creatures.end(); ++iter)
				(*iter)->ForcedDespawn();
		}

		void Reset()
		{
			instance->SetBossState(DATA_SHANNOX, NOT_STARTED);
			me->RemoveAllAuras();
			me->GetMotionMaster()->MovePath(PATH_SHANNOX,true);
			softEnrage = false;
			riplimbIsRespawning = false;
			hurlSpeer = false;
			events.Reset();

			DespawnCreatures(NPC_CRYSTAL_PRISON, 300.0f);
			DespawnCreatures(NPC_CRYSTAL_TRAP, 300.0f);

			if(pRiplimb != NULL)  // Prevents Crashes
			{
				if (pRiplimb->isDead())
					pRiplimb -> Respawn();
			}else
			{
				pRiplimb = me->SummonCreature(NPC_RIPLIMB, me->GetPositionX()-5
					,me->GetPositionY()-5,me->GetPositionZ(),TEMPSUMMON_MANUAL_DESPAWN);
			};

			if(pRageface != NULL)  // Prevents Crashes
			{
				if (pRageface->isDead())
					pRageface -> Respawn();
			}else
			{
				pRageface = me->SummonCreature(NPC_RAGEFACE, me->GetPositionX()+5
					,me->GetPositionY()+5,me->GetPositionZ(),TEMPSUMMON_MANUAL_DESPAWN);
			};

			//me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_DISABLE_MOVE);  //TODO Only for testing

			_Reset();
		}

		void JustSummoned(Creature* summon)
		{
			summons.Summon(summon);
			summon->setActive(true);

			if(me->isInCombat())
				summon->AI()->DoZoneInCombat();
		}

		void JustReachedHome()
		{
			instance->SetBossState(DATA_SHANNOX, FAIL);
		}
		void KilledUnit(Unit * /*victim*/)
		{
		}

		void JustDied(Unit * /*victim*/)
		{
			DoScriptText(SAY_ON_DEAD, me);

			instance->SetBossState(DATA_SHANNOX, DONE);

			summons.DespawnAll();
			
			DespawnCreatures(NPC_RAGEFACE, 300.0f);
			DespawnCreatures(NPC_RIPLIMB, 300.0f);

			instance->SetData(DATA_CURRENT_ENCOUNTER_PHASE, PHASE_NON);

			_JustDied();
		}

		void EnterCombat(Unit* who)
		{
		
			DoZoneInCombat();
			me->CallForHelp(50);

			instance->SetBossState(DATA_SHANNOX, IN_PROGRESS);

			instance->SetData(DATA_CURRENT_ENCOUNTER_PHASE, PHASE_SHANNOX_HAS_SPEER);

			events.ScheduleEvent(EVENT_IMMOLTATION_TRAP, 10000);
			events.ScheduleEvent(EVENT_ARCING_SLASH, 12000);
			events.ScheduleEvent(EVENT_HURL_SPEAR_OR_MAGMA_RUPTUTRE, 20000);
			events.ScheduleEvent(EVENT_SUMMON_CRYSTAL_PRISON, 25000);
			events.ScheduleEvent(EVENT_BERSERK, 10 * MINUTE * IN_MILLISECONDS);

			DoScriptText(SAY_AGGRO, me, who);

			// Sets the current Position as Home Position prevents that Shannox is running through the Instance
			me->SetHomePosition(me->GetPositionX(),me->GetPositionY(),me->GetPositionZ(),me->GetOrientation());


			_EnterCombat();
		}

		void UpdateAI(const uint32 diff)
		{
			if (!me->getVictim()) {}

			events.Update(diff);

			if (me->HasUnitState(UNIT_STAT_CASTING))
				return;

			if(hurlSpeer)
			{
				hurlSpeer = false;
				me->CastSpell(pRiplimb->GetPositionX()+urand(0,500),pRiplimb->GetPositionY()+urand(0,500),
					pRiplimb->GetPositionZ(),SPELL_HURL_SPEAR_SUMMON,true);
				instance->SetData(DATA_CURRENT_ENCOUNTER_PHASE, PHASE_SPEAR_ON_THE_GROUND);

			}

			while (uint32 eventId = events.ExecuteEvent())
			{
				switch (eventId)
				{
				case EVENT_IMMOLTATION_TRAP:
					DoCastVictim(SPELL_IMMOLATION_TRAP,true); // Random Target or Victim?
					events.ScheduleEvent(EVENT_IMMOLTATION_TRAP, 10000);
					break;

				case EVENT_BERSERK:
					DoCast(me, SPELL_BERSERK);
					break;

				case EVENT_ARCING_SLASH:
					DoCastVictim(RAID_MODE(SPELL_ARCTIC_SLASH_10N, SPELL_ARCTIC_SLASH_25N,
						SPELL_ARCTIC_SLASH_10H, SPELL_ARCTIC_SLASH_25H));
					events.ScheduleEvent(EVENT_ARCING_SLASH, 12000);
					break;

				case EVENT_HURL_SPEAR_OR_MAGMA_RUPTUTRE:

					if(pRiplimb->isDead())
					{ // Cast Magma Rupture when Ripclimb is Death
						DoCastVictim(SPELL_MAGMA_RUPTURE_SHANNOX);
						events.ScheduleEvent(EVENT_HURL_SPEAR_OR_MAGMA_RUPTUTRE, 42000);
					}else
					{
						// Throw Spear if Riplimb is Alive and Shannox has the Spear
						if (instance->GetData(DATA_CURRENT_ENCOUNTER_PHASE) == PHASE_SHANNOX_HAS_SPEER)
						{
							events.ScheduleEvent(EVENT_HURL_SPEAR_OR_MAGMA_RUPTUTRE, 42000);

							hurlSpeer = true;

							DoCast(pRiplimb ,SPELL_HURL_SPEAR);

						}else
							// Shifts the Event back if Shannox has not the Spear yet
							events.ScheduleEvent(EVENT_HURL_SPEAR_OR_MAGMA_RUPTUTRE, 10000);
					}

					break;

				case EVENT_SUMMON_SPEAR:

					me->CastSpell(pRiplimb->GetPositionX()+(urand(0,2000)-1000),pRiplimb->GetPositionY()+(urand(0,2000)-1000),
						pRiplimb->GetPositionZ(),SPELL_HURL_SPEAR_SUMMON,true);

					//DoCast(SPELL_HURL_SPEAR_DUMMY_SCRIPT);

					break;

				case EVENT_SUMMON_CRYSTAL_PRISON:
					if (Unit* tempTarget = SelectTarget(SELECT_TARGET_RANDOM, 0, 500, true))
						me->SummonCreature(NPC_CRYSTAL_TRAP, tempTarget->GetPositionX()
						,tempTarget->GetPositionY(),tempTarget->GetPositionZ(),TEMPSUMMON_MANUAL_DESPAWN);
					events.ScheduleEvent(EVENT_SUMMON_CRYSTAL_PRISON, 25000);
					break;

				case EVENT_RIPLIMB_RESPAWN_H:
					riplimbIsRespawning = false;
					pRiplimb->Respawn();
					DoZoneInCombat();
					break;

				default:
					break;
				}
			}

			if (!UpdateVictim())
				return;

			if ((pRiplimb->isDead() || pRageface -> isDead()) && !softEnrage)
			{
				// Heroic: Respawn Riplimb 30s after he is Death
				if(pRiplimb->isDead() && me->GetMap()->IsHeroic() && (!riplimbIsRespawning))
				{
					riplimbIsRespawning = true;
					events.ScheduleEvent(EVENT_RIPLIMB_RESPAWN_H, 30000);
				}

				DoCast(me, SPELL_FRENZY_SHANNOX);
				me->MonsterTextEmote(SAY_ON_DOGS_FALL, 0, true);
				me->MonsterYell(SAY_SOFT_ENRAGE,0,0);
				softEnrage = true;
			}

			if((pRiplimb->GetDistance2d(me) >= maxDistanceBetweenShannoxAndDogs || pRageface->GetDistance2d(me) >= maxDistanceBetweenShannoxAndDogs) && (!me->HasAura(SPELL_SEPERATION_ANXIETY)))
				DoCast(me, SPELL_SEPERATION_ANXIETY);

			if (instance->GetData(DATA_CURRENT_ENCOUNTER_PHASE) == PHASE_RIPLIMB_BRINGS_SPEER && pRiplimb -> GetDistance(me) <= 1)
			{

				instance->SetData(DATA_CURRENT_ENCOUNTER_PHASE, PHASE_SHANNOX_HAS_SPEER);
			}

			DoMeleeAttackIfReady();
		}
	};
};


//Rageface 

class npc_rageface : public CreatureScript
{
public:
	npc_rageface() : CreatureScript("npc_rageface"){}

	CreatureAI* GetAI(Creature* creature) const
	{
		return new npc_ragefaceAI(creature);
	}

	struct npc_ragefaceAI : public ScriptedAI
	{
		npc_ragefaceAI(Creature *c) : ScriptedAI(c)
		{
			instance = me->GetInstanceScript();

			Reset();
		}

		InstanceScript* instance;
		EventMap events;
		Unit* shallTarget;
		bool frenzy;

		void Reset()
		{
			me->RemoveAllAuras();
			events.Reset();
			frenzy = false;
			shallTarget = NULL;

			if(GetShannox() != NULL)
				me->GetMotionMaster()->MoveFollow(GetShannox(), walkRagefaceDistance, walkRagefaceAngle);
		}

		void KilledUnit(Unit * /*victim*/)
		{
		}

		void JustDied(Unit * /*victim*/)
		{
		}

		void EnterCombat(Unit * /*who*/)
		{
			DoZoneInCombat();
			me->CallForHelp(50);

			me->GetMotionMaster()->MoveChase(me->getVictim());

			events.ScheduleEvent(EVENT_FACE_RAGE, 31000);
		}

		void SelectNewTarget()
		{
			shallTarget = SelectTarget(SELECT_TARGET_RANDOM, 1, 500, true);
			me->getThreatManager().resetAllAggro();
			me->AddThreat(shallTarget, 500.0f);
			me->Attack(shallTarget, true);
		}

		void DamageTaken(Unit* who, uint32& damage)
		{

			if (damage >= damageNeededThatRagefaceChangesTarget /* && me->HasAura(BUFF_FACE_RAGE)*/)
			{	
				me->RemoveAura(SPELL_FACE_RAGE);
				me->getVictim()->ClearUnitState(UNIT_STAT_STUNNED);
				me->getVictim()->RemoveAurasDueToSpell(SPELL_FACE_RAGE);
				SelectNewTarget();
			}
		}

		void UpdateAI(const uint32 diff)
		{
			if(!me->isInCombat() && GetShannox() != NULL)
				me->SetOrientation(GetShannox()->GetOrientation());

			if (me->getVictim() != NULL)
			{
				if(!me->HasAura(SPELL_FACE_RAGE) && me->getVictim()->HasAura(SPELL_FACE_RAGE))
				me->getVictim()->RemoveAurasDueToSpell(SPELL_FACE_RAGE);
			}

			events.Update(diff);

			while (uint32 eventId = events.ExecuteEvent())
			{
				switch (eventId)
				{
				case EVENT_FACE_RAGE:
					DoCastVictim(SPELL_FACE_RAGE);
					events.ScheduleEvent(EVENT_FACE_RAGE, 31000);

					// Is this neeeded?
					//me->getVictim()->SetFlag(UNIT_FIELD_FLAGS,  UNIT_STAT_STUNNED);
					//me->getVictim()->AddUnitState(UNIT_STAT_STUNNED);

					break;
				default:
					break;
				}
			}

			if(GetShannox() != NULL)
			{
				if(GetShannox()->GetHealthPct() <= 30 && frenzy == false)
				{
					frenzy = true;
					DoCast(me, SPELL_FRENZIED_DEVOLUTION);
				}

				if(GetShannox()->GetDistance2d(me) >= maxDistanceBetweenShannoxAndDogs && !me->HasAura(SPELL_SEPERATION_ANXIETY)) //TODO Sniff right Distance
				{
					DoCast(me, SPELL_SEPERATION_ANXIETY);
				}

			}

			if (!UpdateVictim())
				return;

			DoMeleeAttackIfReady();
		}

		void DamageDealt(Unit* victim, uint32& damage, DamageEffectType /*damageType*/)
		{
			// Feeding Frenzy (Heroic Ability)
			if(me->GetMap()->IsHeroic() && damage > 0)
				DoCast(me, SPELL_FEEDING_FRENZY_H);
		}

		Creature* GetShannox()
		{
			return ObjectAccessor::GetCreature(*me, instance->GetData64(NPC_SHANNOX));
		}

	};
};

//Riplimb

class npc_riplimb : public CreatureScript
{
public:
	npc_riplimb() : CreatureScript("npc_riplimb"){}

	CreatureAI* GetAI(Creature* creature) const
	{
		return new npc_riplimbAI(creature);
	}

	struct npc_riplimbAI : public ScriptedAI
	{
		npc_riplimbAI(Creature *c) : ScriptedAI(c), vehicle(c->GetVehicleKit())
		{
			instance = me->GetInstanceScript();

			Reset();
		}

		InstanceScript* instance;
		EventMap events;
		bool frenzy;
		bool movementResetNeeded;
		bool inTakingSpearPhase;
		Vehicle* vehicle;

		void Reset()
		{
			me->RemoveAllAuras();
			events.Reset();
			frenzy = false; // Is needed, that Frenzy is not casted twice on Riplimb
			movementResetNeeded = false; // Is needed for correct execution of the Phases
			inTakingSpearPhase = false; // Is needed for correct execution of the Phases

			if(GetShannox() != NULL)
				me->GetMotionMaster()->MoveFollow(GetShannox(), walkRiplimbDistance, walkRiplimbAngle);
		}

		void KilledUnit(Unit * /*victim*/)
		{
		}

		void JustDied(Unit * /*victim*/)
		{
		}

		void EnterCombat(Unit * who)
		{	
			DoZoneInCombat();
			me->CallForHelp(50);

			me->GetMotionMaster()->MoveChase(me->getVictim());

			events.ScheduleEvent(EVENT_LIMB_RIP, 12000); //TODO Find out the correct Time
		}

		void UpdateAI(const uint32 diff)
		{
			if (!me->getVictim()) {}

			if(!me->isInCombat() && GetShannox() != NULL)
				me->SetOrientation(GetShannox()->GetOrientation());

			events.Update(diff);

			while (uint32 eventId = events.ExecuteEvent())
			{
				switch (eventId)
				{
				case EVENT_LIMB_RIP:
					DoCastVictim(SPELL_LIMB_RIP);	
					events.ScheduleEvent(EVENT_LIMB_RIP, 12000); //TODO Find out the correct Time
					break;
				case EVENT_TAKING_SPEAR_DELAY:
					inTakingSpearPhase = false;
					instance->SetData(DATA_CURRENT_ENCOUNTER_PHASE, PHASE_RIPLIMB_GOS_TO_SPEER);
					me->GetMotionMaster()->MovePoint(0,GetSpear()->GetPositionX(),GetSpear()->GetPositionY(),GetSpear()->GetPositionZ());
					break;
				default:
					break;
				}
			}

			if(GetShannox() != NULL)
			{
				if(GetShannox()->GetHealthPct() <= 30 && frenzy == false)
				{
					frenzy = true;
					DoCast(me, SPELL_FRENZIED_DEVOLUTION);
				}

				if(GetShannox()->GetDistance2d(me) >= maxDistanceBetweenShannoxAndDogs && !me->HasAura(SPELL_SEPERATION_ANXIETY)) //TODO Sniff right Distance
				{
					DoCast(me, SPELL_SEPERATION_ANXIETY);
				}
				if(!me->HasAura(SPELL_WARY_10N))
				{
					if (instance->GetData(DATA_CURRENT_ENCOUNTER_PHASE) == PHASE_SPEAR_ON_THE_GROUND && (!inTakingSpearPhase))
					{
						inTakingSpearPhase = true;
						events.ScheduleEvent(EVENT_TAKING_SPEAR_DELAY, 3500);
					}

					if (instance->GetData(DATA_CURRENT_ENCOUNTER_PHASE) == PHASE_RIPLIMB_GOS_TO_SPEER && GetSpear()->GetDistance(me) <= 1)
					{
						instance->SetData(DATA_CURRENT_ENCOUNTER_PHASE, PHASE_RIPLIMB_BRINGS_SPEER);

						GetSpear()->EnterVehicle(me, 1);

						movementResetNeeded = true;
						DoCast(me, SPELL_DOGGED_DETERMINATION);
						me->GetMotionMaster()->MovePoint(0,GetShannox()->GetPositionX(),GetShannox()->GetPositionY(),GetShannox()->GetPositionZ());
					}

					if (instance->GetData(DATA_CURRENT_ENCOUNTER_PHASE) == PHASE_SHANNOX_HAS_SPEER && movementResetNeeded)
					{
						GetSpear()->ExitVehicle();
						movementResetNeeded = false;
						me->RemoveAura(SPELL_DOGGED_DETERMINATION);
						me->GetMotionMaster()->MoveChase(me->getVictim());
					}
				}
			}

			if (!UpdateVictim())
				return;

			DoMeleeAttackIfReady();
		}

		void DamageDealt(Unit* victim, uint32& damage, DamageEffectType /*damageType*/)
		{
			// Feeding Frenzy (Heroic Ability)
			if(me->GetMap()->IsHeroic() && damage > 0)
				DoCast(me, SPELL_FEEDING_FRENZY_H);
		}
		
		inline Creature* GetShannox()
		{
			return ObjectAccessor::GetCreature(*me, instance->GetData64(NPC_SHANNOX));
		}

		inline Creature* GetSpear()
		{
			return ObjectAccessor::GetCreature(*me, instance->GetData64(NPC_SHANNOX_SPEAR));
		}
	};
};

//Shannox Spear

class npc_shannox_spear : public CreatureScript
{
public:
	npc_shannox_spear() : CreatureScript("npc_shannox_spear"){}

	CreatureAI* GetAI(Creature* creature) const
	{
		return new npc_shannox_spearAI(creature);
	}

	struct npc_shannox_spearAI : public ScriptedAI
	{
		npc_shannox_spearAI(Creature *c) : ScriptedAI(c)
		{
			instance = me->GetInstanceScript();

			me->SetReactState(REACT_PASSIVE);
			me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_DISABLE_MOVE | UNIT_FLAG_NOT_SELECTABLE);
			me->SetOrientation(1.45f);
		}

		InstanceScript* instance;

		void Reset()
		{
		}

		void JustDied(Unit * /*victim*/)
		{
		}

		void EnterCombat(Unit * /*who*/)
		{

			/*

			me->GetMotionMaster()->MoveJump(GetRiplimb()->GetPositionX()
			,GetRiplimb()->GetPositionY(),GetRiplimb()->GetPositionZ(),5,1);

			for(int i=0;i<10;i++)
			{
			me->CastSpell(me->GetPositionX()+(urand(0,40)-20),me->GetPositionY()+(urand(0,40)-20),
			46,SPELL_MAGMA_RUPTURE_VISUAL,true);
			}
			*/
			if (GetRiplimb() != NULL)
			{

				float X;
				float Y;

				// Calcs 3 Circles
				for(float width = 6; width <= 26; width = width+10)
				{
					for(float degree = 0; width <= 360; width = width+36)
					{
						Y = sin(degree)*width;
						X = sqrt(width*width+Y*Y);
						me->CastSpell(me->GetPositionX()+X,me->GetPositionY()+Y,
							me->GetPositionZ(),SPELL_MAGMA_RUPTURE_VISUAL,true);

					}
				}


			}


			DoCast(SPELL_MAGMA_FLARE);
		}

		void UpdateAI(const uint32 diff)
		{
			
			if (instance->GetData(DATA_CURRENT_ENCOUNTER_PHASE) == PHASE_RIPLIMB_BRINGS_SPEER)
			{
				instance->SetData(DATA_CURRENT_ENCOUNTER_PHASE, PHASE_RIPLIMB_BRINGS_SPEER);		
				
			}

			if (instance->GetData(DATA_CURRENT_ENCOUNTER_PHASE) == PHASE_SHANNOX_HAS_SPEER)
			{	
				me -> DisappearAndDie();
			}

			if (!UpdateVictim())
				return;
		}

		Creature* GetShannox()
		{
			return ObjectAccessor::GetCreature(*me, instance->GetData64(NPC_SHANNOX));
		}

		Creature* GetRiplimb()
		{
			return ObjectAccessor::GetCreature(*me, instance->GetData64(NPC_RIPLIMB));
		}
	};
};

//Crystal Trap

class npc_crystal_trap : public CreatureScript
{
public:
	npc_crystal_trap() : CreatureScript("npc_crystal_trap"){}

	CreatureAI* GetAI(Creature* creature) const
	{
		return new npc_crystal_trapAI(creature);
	}

	struct npc_crystal_trapAI : public Scripted_NoMovementAI
	{
		npc_crystal_trapAI(Creature *c) : Scripted_NoMovementAI(c)
		{
			instance = me->GetInstanceScript();
			tempTarget = NULL;
			myPrison = NULL;
			//me->SetReactState(REACT_PASSIVE);
			me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_DISABLE_MOVE /*| UNIT_FLAG_NOT_SELECTABLE*/);
			events.Reset();
		}

		InstanceScript* instance;
		EventMap events;
		Unit* tempTarget;
		Creature* myPrison;

		void JustDied(Unit * /*victim*/)
		{
		}

		void Reset()
		{
			events.Reset();
		}

		void EnterCombat(Unit * /*who*/)
		{
			events.ScheduleEvent(EVENT_CRYSTAL_TRAP_TRIGGER, 4000);
			me->SetReactState(REACT_PASSIVE);
		}

		void UpdateAI(const uint32 diff)
		{

			events.Update(diff);

			while (uint32 eventId = events.ExecuteEvent())
			{
				switch (eventId)
				{
				case EVENT_CRYSTAL_TRAP_TRIGGER:

					//Riplimb has a higher Priority than Players...

					if(GetRiplimb() != NULL)
					{
						if(GetRiplimb()->GetDistance(me) <= 1 && (!GetRiplimb()->
							HasAura(SPELL_WARY_10N)) && GetRiplimb()->isAlive()
							&& instance->GetData(DATA_CURRENT_ENCOUNTER_PHASE) == PHASE_SHANNOX_HAS_SPEER)
						{
							tempTarget = GetRiplimb();

						}else
						{
							if (SelectTarget(SELECT_TARGET_NEAREST, 0, 2, true) != NULL)
							{ 
								if (SelectTarget(SELECT_TARGET_NEAREST, 0, 2, true)->GetDistance(me) <= 2)
								{ 
									tempTarget = SelectTarget(SELECT_TARGET_NEAREST, 0, 2, true);
								}
							}
						}
					}
					if (tempTarget == NULL) // If no Target exists try to get a new Target in 0,5s
					{
						events.ScheduleEvent(EVENT_CRYSTAL_TRAP_TRIGGER, 500);
					}else
					{ // Intialize Prison if tempTarget was set
						myPrison = me->SummonCreature(NPC_CRYSTAL_PRISON,me->GetPositionX()
							,me->GetPositionY(),me->GetPositionZ(),0, TEMPSUMMON_MANUAL_DESPAWN);
						myPrison->SetReactState(REACT_PASSIVE);
						DoCast(tempTarget,CRYSTAL_PRISON_EFFECT);
						events.ScheduleEvent(EVENT_CRYSTAL_PRISON_DESPAWN, 15000);
					}

					break;

				case EVENT_CRYSTAL_PRISON_DESPAWN:

					myPrison -> DespawnOrUnsummon();
					tempTarget -> RemoveAurasDueToSpell(CRYSTAL_PRISON_EFFECT);
					// Cast Spell Wary on Ripclimb
					if(tempTarget->GetEntry() == NPC_RIPLIMB)
						DoCast(tempTarget,SPELL_WARY_10N, true);

					me->DisappearAndDie();

					break;

				default:
					break;
				}
			}	

			if(myPrison != NULL)
			{
				if(myPrison->isDead())
				{
					myPrison -> DespawnOrUnsummon();
					tempTarget -> RemoveAurasDueToSpell(CRYSTAL_PRISON_EFFECT);
					me->DisappearAndDie();
				}	
			}	
		}

		Creature* GetRiplimb()
		{
			return ObjectAccessor::GetCreature(*me, instance->GetData64(NPC_RIPLIMB));
		}
	};
};

//Achiements

//Bucket List (5829) //TODO Currently not Working!
class achievement_bucket_list : public AchievementCriteriaScript
{
public:
	achievement_bucket_list() : AchievementCriteriaScript("achievement_bucket_list")
	{
	}

	bool OnCheck(Player* /*player*/, Unit* target)
	{
		if (!target)
			return false;

		return false;
	}
};

void AddSC_boss_shannox()
{
	new boss_shannox();
	new npc_rageface();
	new npc_riplimb();
	new npc_shannox_spear();
	new npc_crystal_trap();
	new achievement_bucket_list();
}
