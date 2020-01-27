#pragma once
#pragma comment( lib, "bakkesmod.lib" )
#include "bakkesmod/plugin/bakkesmodplugin.h"

#define VERSION "0.20"

struct Popup
{
	float startTime = 0.f;
	std::string text = "";
	Vector2 startLocation = { -1, -1 };
};

class KOTH1v1Plugin : public BakkesMod::Plugin::BakkesModPlugin
{
public:
	map<unsigned long long, int> goalMap;
	unsigned long long lastTouchID = 0LL;
	bool fixing = true;

	virtual void onLoad();
	virtual void onUnload();
	void actionLastGoal();
	void cachePlayerScores(bool clear);
	string getTeamNameByNumber(int teamNumber);
	int getTeamNumberByName(string teamName);
	void OnScoreUpdated(string eventName);
	void OnHitBall(string eventName);
	void OnSpawn(string eventName);
	PriWrapper* getLastGoalPri();
	string getPlayerNameByID(unsigned long long userID);
};

