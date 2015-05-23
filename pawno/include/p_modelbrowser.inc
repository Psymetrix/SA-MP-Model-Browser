/*
	p_modelbrowser by Psymetrix, 2015.

	Search, browse and view all skins, vehicles and objects in-game.
	Intended for use with SA-MP 0.3.7 and presumably above.

	modelids.db - Must exist in the scriptfiles forder for searching to work.

	Natives:
		native IsModelBrowserVisible(playerid);
		native ShowModelBrowser(playerid);
		native HideModelBrowser(playerid);
		native ModelBrowserSearch(playerid, search[]);
		native SetBrowserModelID(playerid, modelid);
		native GetBrowserModelID(playerid);
		native SetBrowserModelZoom(playerid, Float:zoom_level);
		native Float:GetBrowserModelZoom(playerid);
		native SetBrowserModelRot(playerid, Float:rot_x, Float:rot_y, Float:rot_z);
		native GetBrowserModelRot(playerid, &Float:rot_x, &Float:rot_y, &Float:rot_z);

	Internal natives:
		native UpdatePreviewModel(playerid);
		native CreatePlayerTextDraws(playerid);
		native DestroyGlobalTextDraws();
		native db_escape_string(text[]);

	Callbacks:
		OnPlayerExitModelBrowser(playerid); - called when a player presses the "close" button on the browser.

	Hooked callbacks:
		OnFilterScriptInit
		OnFilterScriptExit
		OnGameModeInit
		OnGameModeExit
		OnPlayerConnect
		OnPlayerDisconnect
		OnPlayerClickTextDraw
		OnPlayerClickPlayerTextDraw
		OnDialogResponse

	Changelog:
		Script:
			23 May 2015 (v0.1):
				- Added skin and vehicle searching.
			22 May 2015 (v0.1):
				- First version.

		modelids.db:
			23 May 2015:
				- Added SA-MP objects up to 0.3.7 from SAMP.ide.
				- Added skin ids from wiki.sa-mp.com.
				- Added vehicle ids from wiki.sa-mp.com.			
			22 May 2015:
				- Added all object ids from MTA's objects.xml.
*/

// Database that holds all the object ids and names.
#define BROWSER_OBJECT_DATABASE "modelids.db"

// Maximum length of database searches.
#define MAX_BROWSER_SEARCH_LEN 32

// maximum number of search results. Must be 19 or lower.
#define MAX_BROWSER_SEARCH_RESULTS 19

// Maximum length of model names.
#define MAX_MODEL_NAME 31

// Model used when resetting the preview model.
#define DEFAULT_PREVIEW_MODEL 411

// Vehicle colors when previewed in the browser.
// This prevents vehicles changing color when rotated.
#define BROWSER_MODEL_COLOR1 128 // 0.3x green
#define BROWSER_MODEL_COLOR2 01 // white

// Color of client messages sent by this script.
#define COLOR_BROWSER_SCM 0xCCBB88FF

// Change if dialogs conflict.
#define DIALOG_START_ID 1456

// db_escape_string.
#if !defined MAX_INI_ENTRY_TEXT
	#define MAX_INI_ENTRY_TEXT 80
#endif

#include <a_samp>

// Dialog IDs
enum
{
	DIALOG_SEARCH = DIALOG_START_ID,
	DIALOG_ENTER_MODEL
};

enum E_MODEL_INFO
{
	PlayerText: eTextDraw,
		   		eModelid,
	Float: 		eRotX,
	Float: 		eRotY,
	Float: 		eRotZ,
	Float: 		eZoom
};

static stock
	bool:		  gsHasModelBrowser[MAX_PLAYERS],
	Text:		  gsGlobalTextDraws[27],
	PlayerText:   gsSearchResultTD[MAX_PLAYERS][MAX_BROWSER_SEARCH_RESULTS],
				  gsSearchResultModels[MAX_PLAYERS][MAX_BROWSER_SEARCH_RESULTS],
				  gsPlayerModelInfo[MAX_PLAYERS][E_MODEL_INFO],
	PlayerText:   gsSelectedModelTD[MAX_PLAYERS];

forward OnPlayerExitModelBrowser(playerid);

stock IsModelBrowserVisible(playerid)
{
	return gsHasModelBrowser[playerid];
}

stock ShowModelBrowser(playerid)
{
	if (IsModelBrowserVisible(playerid))
		return 0;

	// show global textdraws first
	for (new i; i < sizeof(gsGlobalTextDraws); i++)
	{
		TextDrawShowForPlayer(playerid, gsGlobalTextDraws[i]);
	}

	CreatePlayerTextDraws(playerid);

	gsHasModelBrowser[playerid] = true;

	UpdatePreviewModel(playerid);
	return 1;
}

stock HideModelBrowser(playerid)
{
	if (!IsModelBrowserVisible(playerid))
		return 0;

	for (new i; i < sizeof(gsGlobalTextDraws); i++)
	{
		TextDrawHideForPlayer(playerid, gsGlobalTextDraws[i]);
	}

	DestroyPlayerTextDraws(playerid);

	gsHasModelBrowser[playerid] = false;
	return 1;
}

stock ModelBrowserSearch(playerid, search[])
{
	// Searching is done from smallest ids to largest (skins, vehicles, objects).

	if (!IsModelBrowserVisible(playerid))
		return 0;

	new searchlen = strlen(search);

	// replace spaces with underscores
	for (new i; i < searchlen; i++)
	{
		if (search[i] == ' ') search[i] = '_';
	}

	db_escape_string(search);

	if (searchlen <= 0 || searchlen > MAX_BROWSER_SEARCH_LEN)
		return 0;

	new DB:dbhandle = db_open(BROWSER_OBJECT_DATABASE);
	if (dbhandle == DB:0)
		return 0;

	new 
		modelid[11], model_val, model_name[MAX_MODEL_NAME],
		count, string[128], DBResult:result;

	// search skins
	format(string, sizeof(string), "SELECT `modelid`, `description` FROM `skins` WHERE `description` LIKE '%%%s%%' ORDER BY modelid ASC LIMIT " #MAX_BROWSER_SEARCH_RESULTS, search);
	result = db_query(dbhandle, string);

	// if any skins were found
	if (db_num_rows(result) > 0)
	{
		do
		{
			db_get_field(result, 0, modelid, sizeof(modelid));
			db_get_field(result, 1, model_name, sizeof(model_name));

			model_val = strval(modelid);

			if (count == 0)
				SetBrowserModelID(playerid, model_val);

			gsSearchResultModels[playerid][count] = model_val;

			format(string, sizeof(string), "~b~~h~~h~~h~%d~w~: ~g~~h~~h~~h~%d  ~w~%s", count + 1, model_val, model_name);
			UpdateSearchResultTextDraw(playerid, count, string, true);

			count ++;
		} while (db_next_row(result));
	}

	db_free_result(result);

	// if search results are smaller than max results search vehicles
	if (count < MAX_BROWSER_SEARCH_RESULTS)
	{
		format(string, sizeof(string), "SELECT * FROM `vehicles` WHERE name LIKE '%%%s%%' ORDER BY modelid ASC LIMIT %d", search, MAX_BROWSER_SEARCH_RESULTS - count);
		result = db_query(dbhandle, string);

		if (db_num_rows(result) > 0)
		{
			do
			{
				db_get_field(result, 0, modelid, sizeof(modelid));
				db_get_field(result, 1, model_name, sizeof(model_name));

				model_val = strval(modelid);

				if (count == 0)
					SetBrowserModelID(playerid, model_val);

				gsSearchResultModels[playerid][count] = model_val;
				
				format(string, sizeof(string), "~b~~h~~h~~h~%d~w~: ~g~~h~~h~~h~%d  ~w~%s", count + 1, model_val, model_name);
				UpdateSearchResultTextDraw(playerid, count, string, true);

				count ++;
			} while (db_next_row(result));
		}
		db_free_result(result);
	}

	// if search results are smaller than max results search obects
	if (count < MAX_BROWSER_SEARCH_RESULTS)
	{
		format(string, sizeof(string), "SELECT * FROM `objects` WHERE name LIKE '%%%s%%' ORDER BY modelid ASC LIMIT %d", search, MAX_BROWSER_SEARCH_RESULTS - count);
		result = db_query(dbhandle, string);

		if (db_num_rows(result) > 0)
		{
			do
			{
				db_get_field(result, 0, modelid, sizeof(modelid));
				db_get_field(result, 1, model_name, sizeof(model_name));

				model_val = strval(modelid);

				if (count == 0)
					SetBrowserModelID(playerid, model_val);

				gsSearchResultModels[playerid][count] = model_val;
				
				format(string, sizeof(string), "~b~~h~~h~~h~%d~w~: ~g~~h~~h~~h~%d  ~w~%s", count + 1, model_val, model_name);
				UpdateSearchResultTextDraw(playerid, count, string, true);

				count ++;
			} while (db_next_row(result));
		}
		db_free_result(result);
	}

	if (count < MAX_BROWSER_SEARCH_RESULTS)
	{
		for (new i = count; i < MAX_BROWSER_SEARCH_RESULTS; i++)
		{
			UpdateSearchResultTextDraw(playerid, i, "None", false);
		}
	}

	db_close(dbhandle);
	return 1;
}

static stock UpdateSearchResultTextDraw(playerid, index, text[], bool:selectable)
{
	if (index < 0 || index >= MAX_BROWSER_SEARCH_RESULTS)
		return 0;

	PlayerTextDrawHide(playerid, gsSearchResultTD[playerid][index]);	
	PlayerTextDrawSetString(playerid, gsSearchResultTD[playerid][index], text);
	PlayerTextDrawSetSelectable(playerid, gsSearchResultTD[playerid][index], selectable);
	PlayerTextDrawShow(playerid, gsSearchResultTD[playerid][index]);
	return 1;
}

stock SetBrowserModelID(playerid, modelid)
{
	if (modelid < 0)
		modelid = 0;

	if (modelid > 19999)
		modelid = 19999;

	gsPlayerModelInfo[playerid][eModelid] = modelid;

	UpdatePreviewModel(playerid);
	return 1;
}

stock GetBrowserModelID(playerid)
{
	return gsPlayerModelInfo[playerid][eModelid];
}

stock SetBrowserModelZoom(playerid, Float:zoom_level)
{
	if (zoom_level < -1.4)
		zoom_level = -1.4;

	if (zoom_level > 5.4)
		zoom_level = 5.4;

	gsPlayerModelInfo[playerid][eZoom] = zoom_level;

	UpdatePreviewModel(playerid);
	return 1;
}

stock Float:GetBrowserModelZoom(playerid)
{
	return gsPlayerModelInfo[playerid][eZoom];
}

stock SetBrowserModelRot(playerid, Float:rot_x, Float:rot_y, Float:rot_z)
{
	gsPlayerModelInfo[playerid][eRotX] = rot_x;
	gsPlayerModelInfo[playerid][eRotY] = rot_y;
	gsPlayerModelInfo[playerid][eRotZ] = rot_z;

	UpdatePreviewModel(playerid);
	return 1;
}

stock GetBrowserModelRot(playerid, &Float:rot_x, &Float:rot_y, &Float:rot_z)
{
	if (!IsModelBrowserVisible(playerid))
		return 0;

	rot_x = gsPlayerModelInfo[playerid][eRotX];
	rot_y = gsPlayerModelInfo[playerid][eRotY];
	rot_z = gsPlayerModelInfo[playerid][eRotZ];
	return 1;
}

static stock UpdatePreviewModel(playerid)
{
	if (!IsModelBrowserVisible(playerid))
		return 0;

	new string[26];
	
	PlayerTextDrawHide(playerid, gsPlayerModelInfo[playerid][eTextDraw]);

	PlayerTextDrawSetPreviewModel(playerid, gsPlayerModelInfo[playerid][eTextDraw], gsPlayerModelInfo[playerid][eModelid]);
	PlayerTextDrawSetPreviewRot(playerid, gsPlayerModelInfo[playerid][eTextDraw], gsPlayerModelInfo[playerid][eRotX], gsPlayerModelInfo[playerid][eRotY], gsPlayerModelInfo[playerid][eRotZ], gsPlayerModelInfo[playerid][eZoom]);
	PlayerTextDrawSetPreviewVehCol(playerid, gsPlayerModelInfo[playerid][eTextDraw], BROWSER_MODEL_COLOR1, BROWSER_MODEL_COLOR2);

	PlayerTextDrawShow(playerid, gsPlayerModelInfo[playerid][eTextDraw]);

	format(string, sizeof(string), "~g~~h~~h~~h~%d", GetBrowserModelID(playerid));

	PlayerTextDrawSetString(playerid, gsSelectedModelTD[playerid], string);
	return 1;
}

static stock CreatePlayerTextDraws(playerid)
{
	if (IsModelBrowserVisible(playerid))
		return 0;

	for (new i; i < sizeof(gsSearchResultTD[]); i++)
	{
		gsSearchResultTD[playerid][i] = CreatePlayerTextDraw(playerid, 97.437774, (131.083343 + (10 * i)), "None");
		PlayerTextDrawLetterSize(playerid, gsSearchResultTD[playerid][i], 0.194786, 1.034163);
		PlayerTextDrawTextSize(playerid, gsSearchResultTD[playerid][i], 230.000000, 7.000000);
		PlayerTextDrawAlignment(playerid, gsSearchResultTD[playerid][i], 1);
		PlayerTextDrawColor(playerid, gsSearchResultTD[playerid][i], -1);
		PlayerTextDrawSetShadow(playerid, gsSearchResultTD[playerid][i], 0);
		PlayerTextDrawSetOutline(playerid, gsSearchResultTD[playerid][i], 1);
		PlayerTextDrawBackgroundColor(playerid, gsSearchResultTD[playerid][i], 255);
		PlayerTextDrawFont(playerid, gsSearchResultTD[playerid][i], 1);
		PlayerTextDrawSetProportional(playerid, gsSearchResultTD[playerid][i], 1);
		PlayerTextDrawSetShadow(playerid, gsSearchResultTD[playerid][i], 0);

		PlayerTextDrawShow(playerid, gsSearchResultTD[playerid][i]);
	}

	// model preview
	gsPlayerModelInfo[playerid][eTextDraw] = CreatePlayerTextDraw(playerid, 293.209564, 139.583358, " ");
	PlayerTextDrawLetterSize(playerid, gsPlayerModelInfo[playerid][eTextDraw], 0.000000, 0.000000);
	PlayerTextDrawTextSize(playerid, gsPlayerModelInfo[playerid][eTextDraw], 180.000000, 180.000000);
	PlayerTextDrawAlignment(playerid, gsPlayerModelInfo[playerid][eTextDraw], 1);
	PlayerTextDrawColor(playerid, gsPlayerModelInfo[playerid][eTextDraw], -1);
	PlayerTextDrawSetShadow(playerid, gsPlayerModelInfo[playerid][eTextDraw], 0);
	PlayerTextDrawSetOutline(playerid, gsPlayerModelInfo[playerid][eTextDraw], 0);
	PlayerTextDrawBackgroundColor(playerid, gsPlayerModelInfo[playerid][eTextDraw], 0);
	PlayerTextDrawFont(playerid, gsPlayerModelInfo[playerid][eTextDraw], 5);
	PlayerTextDrawSetProportional(playerid, gsPlayerModelInfo[playerid][eTextDraw], 1);
	PlayerTextDrawSetShadow(playerid, gsPlayerModelInfo[playerid][eTextDraw], 0);
	PlayerTextDrawSetPreviewModel(playerid, gsPlayerModelInfo[playerid][eTextDraw], gsPlayerModelInfo[playerid][eModelid]);
	PlayerTextDrawSetPreviewRot(playerid, gsPlayerModelInfo[playerid][eTextDraw], gsPlayerModelInfo[playerid][eRotX], gsPlayerModelInfo[playerid][eRotY], gsPlayerModelInfo[playerid][eRotZ], gsPlayerModelInfo[playerid][eZoom]);
	
	PlayerTextDrawShow(playerid, gsPlayerModelInfo[playerid][eTextDraw]);

	// selected model id
	gsSelectedModelTD[playerid] = CreatePlayerTextDraw(playerid, 386.578704, 303.750030, "-");
	PlayerTextDrawLetterSize(playerid, gsSelectedModelTD[playerid], 0.310344, 1.714997);
	PlayerTextDrawTextSize(playerid, gsSelectedModelTD[playerid], 15.000000, 215.0);
	PlayerTextDrawAlignment(playerid, gsSelectedModelTD[playerid], 2);
	PlayerTextDrawColor(playerid, gsSelectedModelTD[playerid], -1);
	PlayerTextDrawUseBox(playerid, gsSelectedModelTD[playerid], 1);
	PlayerTextDrawBoxColor(playerid, gsSelectedModelTD[playerid], 0x15151522);
	PlayerTextDrawSetShadow(playerid, gsSelectedModelTD[playerid], 0);
	PlayerTextDrawSetOutline(playerid, gsSelectedModelTD[playerid], 1);
	PlayerTextDrawBackgroundColor(playerid, gsSelectedModelTD[playerid], 255);
	PlayerTextDrawFont(playerid, gsSelectedModelTD[playerid], 2);
	PlayerTextDrawSetProportional(playerid, gsSelectedModelTD[playerid], 1);
	PlayerTextDrawSetShadow(playerid, gsSelectedModelTD[playerid], 0);	
	PlayerTextDrawSetSelectable(playerid, gsSelectedModelTD[playerid], true);
	
	PlayerTextDrawShow(playerid, gsSelectedModelTD[playerid]);
	return 1;
}

static stock DestroyPlayerTextDraws(playerid)
{
	if (!IsModelBrowserVisible(playerid))
		return 0;

	for (new i; i < sizeof(gsSearchResultTD[]); i++)
	{
		PlayerTextDrawHide(playerid, gsSearchResultTD[playerid][i]);
		PlayerTextDrawDestroy(playerid, gsSearchResultTD[playerid][i]);
	}
	PlayerTextDrawHide(playerid, gsPlayerModelInfo[playerid][eTextDraw]);
	PlayerTextDrawDestroy(playerid, gsPlayerModelInfo[playerid][eTextDraw]);

	PlayerTextDrawHide(playerid, gsSelectedModelTD[playerid]);
	PlayerTextDrawDestroy(playerid, gsSelectedModelTD[playerid]);	
	return 1;
}

static stock CreateGlobalTextDraws()
{
	// background border
	gsGlobalTextDraws[0] = TextDrawCreate(88.937042, 118.916755, "_");
	TextDrawLetterSize(gsGlobalTextDraws[0], 0.391566, 29.584177);
	TextDrawTextSize(gsGlobalTextDraws[0], 551.027099, 6.059998);
	TextDrawAlignment(gsGlobalTextDraws[0], 1);
	TextDrawColor(gsGlobalTextDraws[0], 85);
	TextDrawUseBox(gsGlobalTextDraws[0], 1);
	TextDrawBoxColor(gsGlobalTextDraws[0], 255);
	TextDrawSetShadow(gsGlobalTextDraws[0], 0);
	TextDrawSetOutline(gsGlobalTextDraws[0], 0);
	TextDrawBackgroundColor(gsGlobalTextDraws[0], 255);
	TextDrawFont(gsGlobalTextDraws[0], 1);
	TextDrawSetProportional(gsGlobalTextDraws[0], 1);
	TextDrawSetShadow(gsGlobalTextDraws[0], 0);

	// background
	gsGlobalTextDraws[1] = TextDrawCreate(90.000000, 120.000083, "_");
	TextDrawLetterSize(gsGlobalTextDraws[1], 0.391566, 29.314174);
	TextDrawTextSize(gsGlobalTextDraws[1], 550.000000, 0.000000);
	TextDrawAlignment(gsGlobalTextDraws[1], 1);
	TextDrawColor(gsGlobalTextDraws[1], 85);
	TextDrawUseBox(gsGlobalTextDraws[1], 1);
	TextDrawBoxColor(gsGlobalTextDraws[1], 589505535);
	TextDrawSetShadow(gsGlobalTextDraws[1], 0);
	TextDrawSetOutline(gsGlobalTextDraws[1], 0);
	TextDrawBackgroundColor(gsGlobalTextDraws[1], 255);
	TextDrawFont(gsGlobalTextDraws[1], 1);
	TextDrawSetProportional(gsGlobalTextDraws[1], 1);
	TextDrawSetShadow(gsGlobalTextDraws[1], 0);

	// result background border
	gsGlobalTextDraws[2] = TextDrawCreate(94.626663, 125.833366, "__");
	TextDrawLetterSize(gsGlobalTextDraws[2], 0.514319, 22.225837);
	TextDrawTextSize(gsGlobalTextDraws[2], 219.249557, 0.159999);
	TextDrawAlignment(gsGlobalTextDraws[2], 1);
	TextDrawColor(gsGlobalTextDraws[2], -1);
	TextDrawUseBox(gsGlobalTextDraws[2], 1);
	TextDrawBoxColor(gsGlobalTextDraws[2], 255);
	TextDrawSetShadow(gsGlobalTextDraws[2], 0);
	TextDrawSetOutline(gsGlobalTextDraws[2], 0);
	TextDrawBackgroundColor(gsGlobalTextDraws[2], 255);
	TextDrawFont(gsGlobalTextDraws[2], 1);
	TextDrawSetProportional(gsGlobalTextDraws[2], 1);
	TextDrawSetShadow(gsGlobalTextDraws[2], 0);

	// result background
	gsGlobalTextDraws[3] = TextDrawCreate(94.626663, 335.833374, "__");
	TextDrawLetterSize(gsGlobalTextDraws[3], 0.514319, 4.765839);
	TextDrawTextSize(gsGlobalTextDraws[3], 219.249557, 0.019999);
	TextDrawAlignment(gsGlobalTextDraws[3], 1);
	TextDrawColor(gsGlobalTextDraws[3], -1);
	TextDrawUseBox(gsGlobalTextDraws[3], 1);
	TextDrawBoxColor(gsGlobalTextDraws[3], 255);
	TextDrawSetShadow(gsGlobalTextDraws[3], 0);
	TextDrawSetOutline(gsGlobalTextDraws[3], 0);
	TextDrawBackgroundColor(gsGlobalTextDraws[3], 255);
	TextDrawFont(gsGlobalTextDraws[3], 1);
	TextDrawSetProportional(gsGlobalTextDraws[3], 1);
	TextDrawSetShadow(gsGlobalTextDraws[3], 0);

	gsGlobalTextDraws[4] = TextDrawCreate(95.626663, 127.333366, "__");
	TextDrawLetterSize(gsGlobalTextDraws[4], 0.514319, 21.865840);
	TextDrawTextSize(gsGlobalTextDraws[4], 217.814559, 0.319999);
	TextDrawAlignment(gsGlobalTextDraws[4], 1);
	TextDrawColor(gsGlobalTextDraws[4], -1);
	TextDrawUseBox(gsGlobalTextDraws[4], 1);
	TextDrawBoxColor(gsGlobalTextDraws[4], 269488383);
	TextDrawSetShadow(gsGlobalTextDraws[4], 0);
	TextDrawSetOutline(gsGlobalTextDraws[4], 0);
	TextDrawBackgroundColor(gsGlobalTextDraws[4], 255);
	TextDrawFont(gsGlobalTextDraws[4], 1);
	TextDrawSetProportional(gsGlobalTextDraws[4], 1);
	TextDrawSetShadow(gsGlobalTextDraws[4], 0);

	gsGlobalTextDraws[5] = TextDrawCreate(95.626663, 337.333374, "__");
	TextDrawLetterSize(gsGlobalTextDraws[5], 0.514319, 4.405838);
	TextDrawTextSize(gsGlobalTextDraws[5], 217.814559, 0.319999);
	TextDrawAlignment(gsGlobalTextDraws[5], 1);
	TextDrawColor(gsGlobalTextDraws[5], -1);
	TextDrawUseBox(gsGlobalTextDraws[5], 1);
	TextDrawBoxColor(gsGlobalTextDraws[5], 269488383);
	TextDrawSetShadow(gsGlobalTextDraws[5], 0);
	TextDrawSetOutline(gsGlobalTextDraws[5], 0);
	TextDrawBackgroundColor(gsGlobalTextDraws[5], 255);
	TextDrawFont(gsGlobalTextDraws[5], 1);
	TextDrawSetProportional(gsGlobalTextDraws[5], 1);
	TextDrawSetShadow(gsGlobalTextDraws[5], 0);

	// search button
	gsGlobalTextDraws[6] = TextDrawCreate(97.437774, 339.083343, "Search");
	TextDrawLetterSize(gsGlobalTextDraws[6], 0.194786, 1.034163);
	TextDrawTextSize(gsGlobalTextDraws[6], 132.000000, 11.000000);
	TextDrawAlignment(gsGlobalTextDraws[6], 1);
	TextDrawColor(gsGlobalTextDraws[6], -1);
	TextDrawSetShadow(gsGlobalTextDraws[6], 0);
	TextDrawSetOutline(gsGlobalTextDraws[6], 1);
	TextDrawBackgroundColor(gsGlobalTextDraws[6], 255);
	TextDrawFont(gsGlobalTextDraws[6], 2);
	TextDrawSetProportional(gsGlobalTextDraws[6], 1);
	TextDrawSetShadow(gsGlobalTextDraws[6], 0);
	TextDrawSetSelectable(gsGlobalTextDraws[6], true);

	gsGlobalTextDraws[7] = TextDrawCreate(97.437774, 349.083343, "");
	TextDrawLetterSize(gsGlobalTextDraws[7], 0.194786, 1.034163);
	TextDrawAlignment(gsGlobalTextDraws[7], 1);
	TextDrawColor(gsGlobalTextDraws[7], -1);
	TextDrawSetShadow(gsGlobalTextDraws[7], 0);
	TextDrawSetOutline(gsGlobalTextDraws[7], 1);
	TextDrawBackgroundColor(gsGlobalTextDraws[7], 255);
	TextDrawFont(gsGlobalTextDraws[7], 2);
	TextDrawSetProportional(gsGlobalTextDraws[7], 1);
	TextDrawSetShadow(gsGlobalTextDraws[7], 0);

	gsGlobalTextDraws[8] = TextDrawCreate(228.626663, 125.833366, "__");
	TextDrawLetterSize(gsGlobalTextDraws[8], 0.808318, 28.105785);
	TextDrawTextSize(gsGlobalTextDraws[8], 545.207153, 0.159999);
	TextDrawAlignment(gsGlobalTextDraws[8], 1);
	TextDrawColor(gsGlobalTextDraws[8], -1);
	TextDrawUseBox(gsGlobalTextDraws[8], 1);
	TextDrawBoxColor(gsGlobalTextDraws[8], 255);
	TextDrawSetShadow(gsGlobalTextDraws[8], 0);
	TextDrawSetOutline(gsGlobalTextDraws[8], 0);
	TextDrawBackgroundColor(gsGlobalTextDraws[8], 255);
	TextDrawFont(gsGlobalTextDraws[8], 1);
	TextDrawSetProportional(gsGlobalTextDraws[8], 1);
	TextDrawSetShadow(gsGlobalTextDraws[8], 0);

	gsGlobalTextDraws[9] = TextDrawCreate(230.126663, 127.333366, "__");
	TextDrawLetterSize(gsGlobalTextDraws[9], 0.514319, 27.745788);
	TextDrawTextSize(gsGlobalTextDraws[9], 543.416198, 0.319999);
	TextDrawAlignment(gsGlobalTextDraws[9], 1);
	TextDrawColor(gsGlobalTextDraws[9], -1);
	TextDrawUseBox(gsGlobalTextDraws[9], 1);
	TextDrawBoxColor(gsGlobalTextDraws[9], 269488383);
	TextDrawSetShadow(gsGlobalTextDraws[9], 0);
	TextDrawSetOutline(gsGlobalTextDraws[9], 0);
	TextDrawBackgroundColor(gsGlobalTextDraws[9], 255);
	TextDrawFont(gsGlobalTextDraws[9], 1);
	TextDrawSetProportional(gsGlobalTextDraws[9], 1);
	TextDrawSetShadow(gsGlobalTextDraws[9], 0);

	gsGlobalTextDraws[10] = TextDrawCreate(236.588317, 132.833282, "__");
	TextDrawLetterSize(gsGlobalTextDraws[10], 0.565388, 21.450012);
	TextDrawTextSize(gsGlobalTextDraws[10], 537.375122, 0.159999);
	TextDrawAlignment(gsGlobalTextDraws[10], 1);
	TextDrawColor(gsGlobalTextDraws[10], -1);
	TextDrawUseBox(gsGlobalTextDraws[10], 1);
	TextDrawBoxColor(gsGlobalTextDraws[10], 255);
	TextDrawSetShadow(gsGlobalTextDraws[10], 0);
	TextDrawSetOutline(gsGlobalTextDraws[10], 0);
	TextDrawBackgroundColor(gsGlobalTextDraws[10], 255);
	TextDrawFont(gsGlobalTextDraws[10], 1);
	TextDrawSetProportional(gsGlobalTextDraws[10], 1);
	TextDrawSetShadow(gsGlobalTextDraws[10], 0);

	gsGlobalTextDraws[11] = TextDrawCreate(237.588317, 134.333282, "__");
	TextDrawLetterSize(gsGlobalTextDraws[11], 0.535388, 21.090015);
	TextDrawTextSize(gsGlobalTextDraws[11], 535.825073, 0.159999);
	TextDrawAlignment(gsGlobalTextDraws[11], 1);
	TextDrawColor(gsGlobalTextDraws[11], -1);
	TextDrawUseBox(gsGlobalTextDraws[11], 1);
	TextDrawBoxColor(gsGlobalTextDraws[11], 589505535);
	TextDrawSetShadow(gsGlobalTextDraws[11], 0);
	TextDrawSetOutline(gsGlobalTextDraws[11], 0);
	TextDrawBackgroundColor(gsGlobalTextDraws[11], 255);
	TextDrawFont(gsGlobalTextDraws[11], 1);
	TextDrawSetProportional(gsGlobalTextDraws[11], 1);
	TextDrawSetShadow(gsGlobalTextDraws[11], 0);

	// model id back button
	gsGlobalTextDraws[12] = TextDrawCreate(249.286865, 302.250030, "ld_beat:left");
	TextDrawLetterSize(gsGlobalTextDraws[12], 0.000000, 0.000000);
	TextDrawTextSize(gsGlobalTextDraws[12], 17.403734, 19.130018);
	TextDrawAlignment(gsGlobalTextDraws[12], 1);
	TextDrawColor(gsGlobalTextDraws[12], -1);
	TextDrawSetShadow(gsGlobalTextDraws[12], 0);
	TextDrawSetOutline(gsGlobalTextDraws[12], 0);
	TextDrawBackgroundColor(gsGlobalTextDraws[12], 255);
	TextDrawFont(gsGlobalTextDraws[12], 4);
	TextDrawSetProportional(gsGlobalTextDraws[12], 1);
	TextDrawSetShadow(gsGlobalTextDraws[12], 0);
	TextDrawSetSelectable(gsGlobalTextDraws[12], true);

	// model id forward button
	gsGlobalTextDraws[13] = TextDrawCreate(508.286865, 302.250030, "ld_beat:right");
	TextDrawLetterSize(gsGlobalTextDraws[13], 0.000000, 0.000000);
	TextDrawTextSize(gsGlobalTextDraws[13], 17.403734, 19.130018);
	TextDrawAlignment(gsGlobalTextDraws[13], 1);
	TextDrawColor(gsGlobalTextDraws[13], -1);
	TextDrawSetShadow(gsGlobalTextDraws[13], 0);
	TextDrawSetOutline(gsGlobalTextDraws[13], 0);
	TextDrawBackgroundColor(gsGlobalTextDraws[13], 255);
	TextDrawFont(gsGlobalTextDraws[13], 4);
	TextDrawSetProportional(gsGlobalTextDraws[13], 1);
	TextDrawSetShadow(gsGlobalTextDraws[13], 0);
	TextDrawSetSelectable(gsGlobalTextDraws[13], true);

	gsGlobalTextDraws[14] = TextDrawCreate(384.173004, 358.333404, "zoom");
	TextDrawLetterSize(gsGlobalTextDraws[14], 0.230786, 1.334163);
	TextDrawAlignment(gsGlobalTextDraws[14], 2);
	TextDrawColor(gsGlobalTextDraws[14], -1);
	TextDrawSetShadow(gsGlobalTextDraws[14], 0);
	TextDrawSetOutline(gsGlobalTextDraws[14], 1);
	TextDrawBackgroundColor(gsGlobalTextDraws[14], 255);
	TextDrawFont(gsGlobalTextDraws[14], 2);
	TextDrawSetProportional(gsGlobalTextDraws[14], 1);
	TextDrawSetShadow(gsGlobalTextDraws[14], 0);

	// zoom in button
	gsGlobalTextDraws[15] = TextDrawCreate(340.000000, 355.000000, "ld_beat:left");
	TextDrawLetterSize(gsGlobalTextDraws[15], 0.000000, 0.000000);
	TextDrawTextSize(gsGlobalTextDraws[15], 17.403734, 19.130018);
	TextDrawAlignment(gsGlobalTextDraws[15], 1);
	TextDrawColor(gsGlobalTextDraws[15], -1);
	TextDrawSetShadow(gsGlobalTextDraws[15], 0);
	TextDrawSetOutline(gsGlobalTextDraws[15], 0);
	TextDrawBackgroundColor(gsGlobalTextDraws[15], 255);
	TextDrawFont(gsGlobalTextDraws[15], 4);
	TextDrawSetProportional(gsGlobalTextDraws[15], 1);
	TextDrawSetShadow(gsGlobalTextDraws[15], 0);
	TextDrawSetSelectable(gsGlobalTextDraws[15], true);

	// zoom out button
	gsGlobalTextDraws[16] = TextDrawCreate(411.500000, 355.000000, "ld_beat:right");
	TextDrawLetterSize(gsGlobalTextDraws[16], 0.000000, 0.000000);
	TextDrawTextSize(gsGlobalTextDraws[16], 17.403734, 19.130018);
	TextDrawAlignment(gsGlobalTextDraws[16], 1);
	TextDrawColor(gsGlobalTextDraws[16], -1);
	TextDrawSetShadow(gsGlobalTextDraws[16], 0);
	TextDrawSetOutline(gsGlobalTextDraws[16], 0);
	TextDrawBackgroundColor(gsGlobalTextDraws[16], 255);
	TextDrawFont(gsGlobalTextDraws[16], 4);
	TextDrawSetProportional(gsGlobalTextDraws[16], 1);
	TextDrawSetShadow(gsGlobalTextDraws[16], 0);
	TextDrawSetSelectable(gsGlobalTextDraws[16], true);

	gsGlobalTextDraws[17] = TextDrawCreate(384.173004, 335.333404, "Y");
	TextDrawLetterSize(gsGlobalTextDraws[17], 0.230786, 1.334163);
	TextDrawAlignment(gsGlobalTextDraws[17], 2);
	TextDrawColor(gsGlobalTextDraws[17], -1);
	TextDrawSetShadow(gsGlobalTextDraws[17], 0);
	TextDrawSetOutline(gsGlobalTextDraws[17], 1);
	TextDrawBackgroundColor(gsGlobalTextDraws[17], 255);
	TextDrawFont(gsGlobalTextDraws[17], 2);
	TextDrawSetProportional(gsGlobalTextDraws[17], 1);
	TextDrawSetShadow(gsGlobalTextDraws[17], 0);

	// decrease y rotation button
	gsGlobalTextDraws[18] = TextDrawCreate(340.000000, 332.000000, "ld_beat:left");
	TextDrawLetterSize(gsGlobalTextDraws[18], 0.000000, 0.000000);
	TextDrawTextSize(gsGlobalTextDraws[18], 17.403734, 19.130018);
	TextDrawAlignment(gsGlobalTextDraws[18], 1);
	TextDrawColor(gsGlobalTextDraws[18], -1);
	TextDrawSetShadow(gsGlobalTextDraws[18], 0);
	TextDrawSetOutline(gsGlobalTextDraws[18], 0);
	TextDrawBackgroundColor(gsGlobalTextDraws[18], 255);
	TextDrawFont(gsGlobalTextDraws[18], 4);
	TextDrawSetProportional(gsGlobalTextDraws[18], 1);
	TextDrawSetShadow(gsGlobalTextDraws[18], 0);
	TextDrawSetSelectable(gsGlobalTextDraws[18], true);

	// increase y rotation button
	gsGlobalTextDraws[19] = TextDrawCreate(411.500000, 332.000000, "ld_beat:right");
	TextDrawLetterSize(gsGlobalTextDraws[19], 0.000000, 0.000000);
	TextDrawTextSize(gsGlobalTextDraws[19], 17.403734, 19.130018);
	TextDrawAlignment(gsGlobalTextDraws[19], 1);
	TextDrawColor(gsGlobalTextDraws[19], -1);
	TextDrawSetShadow(gsGlobalTextDraws[19], 0);
	TextDrawSetOutline(gsGlobalTextDraws[19], 0);
	TextDrawBackgroundColor(gsGlobalTextDraws[19], 255);
	TextDrawFont(gsGlobalTextDraws[19], 4);
	TextDrawSetProportional(gsGlobalTextDraws[19], 1);
	TextDrawSetShadow(gsGlobalTextDraws[19], 0);
	TextDrawSetSelectable(gsGlobalTextDraws[19], true);

	gsGlobalTextDraws[20] = TextDrawCreate(288.173004, 335.333404, "X");
	TextDrawLetterSize(gsGlobalTextDraws[20], 0.230786, 1.334163);
	TextDrawAlignment(gsGlobalTextDraws[20], 2);
	TextDrawColor(gsGlobalTextDraws[20], -1);
	TextDrawSetShadow(gsGlobalTextDraws[20], 0);
	TextDrawSetOutline(gsGlobalTextDraws[20], 1);
	TextDrawBackgroundColor(gsGlobalTextDraws[20], 255);
	TextDrawFont(gsGlobalTextDraws[20], 2);
	TextDrawSetProportional(gsGlobalTextDraws[20], 1);
	TextDrawSetShadow(gsGlobalTextDraws[20], 0);

	// decrease x rotation button
	gsGlobalTextDraws[21] = TextDrawCreate(249.500000, 332.000000, "ld_beat:left");
	TextDrawLetterSize(gsGlobalTextDraws[21], 0.000000, 0.000000);
	TextDrawTextSize(gsGlobalTextDraws[21], 17.403734, 19.130018);
	TextDrawAlignment(gsGlobalTextDraws[21], 1);
	TextDrawColor(gsGlobalTextDraws[21], -1);
	TextDrawSetShadow(gsGlobalTextDraws[21], 0);
	TextDrawSetOutline(gsGlobalTextDraws[21], 0);
	TextDrawBackgroundColor(gsGlobalTextDraws[21], 255);
	TextDrawFont(gsGlobalTextDraws[21], 4);
	TextDrawSetProportional(gsGlobalTextDraws[21], 1);
	TextDrawSetShadow(gsGlobalTextDraws[21], 0);
	TextDrawSetSelectable(gsGlobalTextDraws[21], true);

	// increase x rotation button
	gsGlobalTextDraws[22] = TextDrawCreate(309.000000, 332.000000, "ld_beat:right");
	TextDrawLetterSize(gsGlobalTextDraws[22], 0.000000, 0.000000);
	TextDrawTextSize(gsGlobalTextDraws[22], 17.403734, 19.130018);
	TextDrawAlignment(gsGlobalTextDraws[22], 1);
	TextDrawColor(gsGlobalTextDraws[22], -1);
	TextDrawSetShadow(gsGlobalTextDraws[22], 0);
	TextDrawSetOutline(gsGlobalTextDraws[22], 0);
	TextDrawBackgroundColor(gsGlobalTextDraws[22], 255);
	TextDrawFont(gsGlobalTextDraws[22], 4);
	TextDrawSetProportional(gsGlobalTextDraws[22], 1);
	TextDrawSetShadow(gsGlobalTextDraws[22], 0);
	TextDrawSetSelectable(gsGlobalTextDraws[22], true);

	gsGlobalTextDraws[23] = TextDrawCreate(484.673004, 335.333404, "Z");
	TextDrawLetterSize(gsGlobalTextDraws[23], 0.230786, 1.334163);
	TextDrawAlignment(gsGlobalTextDraws[23], 2);
	TextDrawColor(gsGlobalTextDraws[23], -1);
	TextDrawSetShadow(gsGlobalTextDraws[23], 0);
	TextDrawSetOutline(gsGlobalTextDraws[23], 1);
	TextDrawBackgroundColor(gsGlobalTextDraws[23], 255);
	TextDrawFont(gsGlobalTextDraws[23], 2);
	TextDrawSetProportional(gsGlobalTextDraws[23], 1);
	TextDrawSetShadow(gsGlobalTextDraws[23], 0);

	// decrease z rotation button
	gsGlobalTextDraws[24] = TextDrawCreate(443.500000, 332.000000, "ld_beat:left");
	TextDrawLetterSize(gsGlobalTextDraws[24], 0.000000, 0.000000);
	TextDrawTextSize(gsGlobalTextDraws[24], 17.403734, 19.130018);
	TextDrawAlignment(gsGlobalTextDraws[24], 1);
	TextDrawColor(gsGlobalTextDraws[24], -1);
	TextDrawSetShadow(gsGlobalTextDraws[24], 0);
	TextDrawSetOutline(gsGlobalTextDraws[24], 0);
	TextDrawBackgroundColor(gsGlobalTextDraws[24], 255);
	TextDrawFont(gsGlobalTextDraws[24], 4);
	TextDrawSetProportional(gsGlobalTextDraws[24], 1);
	TextDrawSetShadow(gsGlobalTextDraws[24], 0);
	TextDrawSetSelectable(gsGlobalTextDraws[24], true);

	// increase z rotation button
	gsGlobalTextDraws[25] = TextDrawCreate(508.000000, 332.000000, "ld_beat:right");
	TextDrawLetterSize(gsGlobalTextDraws[25], 0.000000, 0.000000);
	TextDrawTextSize(gsGlobalTextDraws[25], 17.403734, 19.130018);
	TextDrawAlignment(gsGlobalTextDraws[25], 1);
	TextDrawColor(gsGlobalTextDraws[25], -1);
	TextDrawSetShadow(gsGlobalTextDraws[25], 0);
	TextDrawSetOutline(gsGlobalTextDraws[25], 0);
	TextDrawBackgroundColor(gsGlobalTextDraws[25], 255);
	TextDrawFont(gsGlobalTextDraws[25], 4);
	TextDrawSetProportional(gsGlobalTextDraws[25], 1);
	TextDrawSetShadow(gsGlobalTextDraws[25], 0);
	TextDrawSetSelectable(gsGlobalTextDraws[25], true);

	// close button
	gsGlobalTextDraws[26] = TextDrawCreate(97.437774, 359.083343, "close");
	TextDrawLetterSize(gsGlobalTextDraws[26], 0.194786, 1.034163);
	TextDrawTextSize(gsGlobalTextDraws[26], 126.000000, 11.000000);
	TextDrawAlignment(gsGlobalTextDraws[26], 1);
	TextDrawColor(gsGlobalTextDraws[26], -1);
	TextDrawSetShadow(gsGlobalTextDraws[26], 0);
	TextDrawSetOutline(gsGlobalTextDraws[26], 1);
	TextDrawBackgroundColor(gsGlobalTextDraws[26], 255);
	TextDrawFont(gsGlobalTextDraws[26], 2);
	TextDrawSetProportional(gsGlobalTextDraws[26], 1);
	TextDrawSetShadow(gsGlobalTextDraws[26], 0);
	TextDrawSetSelectable(gsGlobalTextDraws[26], true);	
	return 1;
}

static stock DestroyGlobalTextDraws()
{
	for (new i; i < sizeof(gsGlobalTextDraws); i++)
	{
		TextDrawHideForAll(gsGlobalTextDraws[i]);
		TextDrawDestroy(gsGlobalTextDraws[i]);
	}
	return 1;
}

static stock db_escape_string(text[])
{
	new
		ret[MAX_INI_ENTRY_TEXT * 2],
		ch,
		i,
		j;
	while ((ch = text[i++]) && j < sizeof (ret))
	{
		if (ch == '\'')
		{
			if (j < sizeof (ret) - 2)
			{
				ret[j++] = '\'';
				ret[j++] = '\'';
			}
		}
		else if (j < sizeof (ret))
		{
			ret[j++] = ch;
		}
		else
		{
			j++;
		}
	}
	ret[sizeof (ret) - 1] = '\0';
	return ret;
}

public OnPlayerConnect(playerid)
{
	SetBrowserModelID(playerid, DEFAULT_PREVIEW_MODEL);
	SetBrowserModelRot(playerid, 0.0, 0.0, 0.0);
	SetBrowserModelZoom(playerid, 1.0);

    #if defined Mb_OnPlayerConnect
        Mb_OnPlayerConnect(playerid);
    #endif
        return 1;
}

public OnPlayerDisconnect(playerid, reason)
{
	HideModelBrowser(playerid);

    #if defined Mb_OnPlayerDisconnect
        Mb_OnPlayerDisconnect(playerid, reason);
    #endif
        return 1;
}

public OnDialogResponse(playerid, dialogid, response, listitem, inputtext[])
{
	if (IsModelBrowserVisible(playerid))
	{
		if (dialogid == DIALOG_SEARCH)
		{
			if (response)
			{
				if (ModelBrowserSearch(playerid, inputtext))
				{
					SendClientMessage(playerid, COLOR_BROWSER_SCM, "Search complete.");
				}
				else
				{
					SendClientMessage(playerid, COLOR_BROWSER_SCM, "Unable to complete model search.");					
				}
			}
		}
		else if (dialogid == DIALOG_ENTER_MODEL)
		{
			if (response)
			{
				SetBrowserModelID(playerid, strval(inputtext));
				SendClientMessage(playerid, COLOR_BROWSER_SCM, "Preview model set.");
			}
		}
	}
    #if defined Mb_OnDialogResponse
        Mb_OnDialogResponse(playerid, dialogid, response, listitem, inputtext);
    #endif
        return 1;
}

public OnPlayerClickTextDraw(playerid, Text:clickedid)
{
	if (IsModelBrowserVisible(playerid))
	{
		new Float:rot_x, Float:rot_y, Float:rot_z;
		GetBrowserModelRot(playerid, rot_x, rot_y, rot_z);

		// player pressed ESC key
		if (clickedid == Text:INVALID_TEXT_DRAW)
		{
			HideModelBrowser(playerid);

			if (funcidx("OnPlayerExitModelBrowser") != -1)
				CallLocalFunction("OnPlayerExitModelBrowser", "i", playerid);			
		}
		// zoom out in
		else if (clickedid == gsGlobalTextDraws[15])
		{
			SetBrowserModelZoom(playerid, GetBrowserModelZoom(playerid) + 0.1);
		}
		// zoom out
		else if (clickedid == gsGlobalTextDraws[16])
		{
			SetBrowserModelZoom(playerid, GetBrowserModelZoom(playerid) - 0.1);
		}
		// x rotation -
		else if (clickedid == gsGlobalTextDraws[21])
		{
			SetBrowserModelRot(playerid, rot_x - 10.0, rot_y, rot_z);
		}
		// x rotation +
		else if (clickedid == gsGlobalTextDraws[22])
		{
			SetBrowserModelRot(playerid, rot_x + 10.0, rot_y, rot_z);
		}
		// y rotation -
		else if (clickedid == gsGlobalTextDraws[18])
		{
			SetBrowserModelRot(playerid, rot_x, rot_y - 10.0, rot_z);
		}
		// y rotation +
		else if (clickedid == gsGlobalTextDraws[19])
		{
			SetBrowserModelRot(playerid, rot_x, rot_y + 10.0, rot_z);
		}
		// z rotation -
		else if (clickedid == gsGlobalTextDraws[24])
		{
			SetBrowserModelRot(playerid, rot_x, rot_y, rot_z - 10.0);
		}
		// z rotation +
		else if (clickedid == gsGlobalTextDraws[25])
		{
			SetBrowserModelRot(playerid, rot_x, rot_y, rot_z + 10.0);
		}
		// previous model
		else if (clickedid == gsGlobalTextDraws[12])
		{
			SetBrowserModelID(playerid, GetBrowserModelID(playerid) - 1);
		}
		// next model
		else if (clickedid == gsGlobalTextDraws[13])
		{
			SetBrowserModelID(playerid, GetBrowserModelID(playerid) + 1);
		}
		// search button
		else if (clickedid == gsGlobalTextDraws[6])
		{
			ShowPlayerDialog(playerid, DIALOG_SEARCH, DIALOG_STYLE_INPUT, "Model search", "Please enter your search. A search can contain up to " #MAX_BROWSER_SEARCH_LEN " characters maximum.", "Search", "Cancel");
		}
		// close button
		else if (clickedid == gsGlobalTextDraws[26])
		{
			HideModelBrowser(playerid);
			if (funcidx("OnPlayerExitModelBrowser") != -1)
				CallLocalFunction("OnPlayerExitModelBrowser", "i", playerid);
		}
	}
    #if defined Mb_OnPlayerClickTextDraw
        Mb_OnPlayerClickTextDraw(playerid, Text:clickedid);
    #endif
    return 1;
}

public OnPlayerClickPlayerTextDraw(playerid, PlayerText:playertextid)
{
	if (IsModelBrowserVisible(playerid))
	{
		if (playertextid == gsSelectedModelTD[playerid])
		{
			ShowPlayerDialog(playerid, DIALOG_ENTER_MODEL, DIALOG_STYLE_INPUT, "Model viewer", "Please enter a model id.\nSkins ids: 0 - 311.\nVehicle ids: 400 - 611.\nSA-MP objects: 11682 - 12799 | 18631 - 19999.", "Confirm", "Cancel");
		}
		else
		{
			for (new i; i < MAX_BROWSER_SEARCH_RESULTS; i++)
			{
				if (playertextid == gsSearchResultTD[playerid][i])
				{
					SetBrowserModelID(playerid, gsSearchResultModels[playerid][i]);
					break;
				}
			}
		}
	}

    #if defined Mb_OnPlayerClickPlayerTD
        Mb_OnPlayerClickPlayerTD(playerid, PlayerText:playertextid);
    #endif
    return 1;
}

//------------------------------------------------------------------------------

#if defined FILTERSCRIPT

public OnFilterScriptInit()
{
	CreateGlobalTextDraws();

    #if defined Mb_OnFilterScriptInit
        Mb_OnFilterScriptInit();
    #endif
	return 1;
}

public OnFilterScriptExit()
{
	for (new i; i < MAX_PLAYERS; i++)
	{
		HideModelBrowser(i);
	}
	DestroyGlobalTextDraws();

    #if defined Mb_OnFilterScriptExit
        Mb_OnFilterScriptExit();
    #endif
	return 1;
}

#else

public OnGameModeInit()
{
	CreateGlobalTextDraws();

    #if defined Mb_OnGameModeInit
        Mb_OnGameModeInit();
    #endif
	return 1;
}

public OnGameModeExit()
{
	for (new i; i < MAX_PLAYERS; i++)
	{
		HideModelBrowser(i);
	}
	DestroyGlobalTextDraws();

    #if defined Mb_OnGameModeExit
        Mb_OnGameModeExit();
    #endif
	return 1;
}

// FILTERSCRIPT
#endif


#if defined FILTERSCRIPT

// OnFilterScriptInit
#if defined _ALS_OnFilterScriptInit
    #undef OnFilterScriptInit
#else
    #define _ALS_OnFilterScriptInit
#endif
#define OnFilterScriptInit Mb_OnFilterScriptInit
#if defined Mb_OnFilterScriptInit
    forward Mb_OnFilterScriptInit();
#endif

// OnFilterScriptExit
#if defined _ALS_OnFilterScriptExit
    #undef OnFilterScriptExit
#else
    #define _ALS_OnFilterScriptExit
#endif
#define OnFilterScriptExit Mb_OnFilterScriptExit
#if defined Mb_OnFilterScriptExit
    forward Mb_OnFilterScriptExit();
#endif

#else

// OnGameModeInit
#if defined _ALS_OnGameModeInit
    #undef OnGameModeInit
#else
    #define _ALS_OnGameModeInit
#endif
#define OnGameModeInit Mb_OnGameModeInit
#if defined Mb_OnGameModeInit
    forward Mb_OnGameModeInit();
#endif

// OnGameModeExit
#if defined _ALS_OnGameModeExit
    #undef OnGameModeExit
#else
    #define _ALS_OnGameModeExit
#endif
#define OnGameModeExit Mb_OnGameModeExit
#if defined Mb_OnGameModeExit
    forward Mb_OnGameModeExit();
#endif

// FILTERSCRIPT
#endif

// OnPlayerConnect
#if defined _ALS_OnPlayerConnect
    #undef OnPlayerConnect
#else
    #define _ALS_OnPlayerConnect
#endif
#define OnPlayerConnect Mb_OnPlayerConnect
#if defined Mb_OnPlayerConnect
    forward Mb_OnPlayerConnect(playerid);
#endif

// OnPlayerDisconnect
#if defined _ALS_OnPlayerDisconnect
    #undef OnPlayerDisconnect
#else
    #define _ALS_OnPlayerDisconnect
#endif
#define OnPlayerDisconnect Mb_OnPlayerDisconnect
#if defined Mb_OnPlayerDisconnect
    forward Mb_OnPlayerDisconnect(playerid, reason);
#endif

// OnDialogResponse
#if defined _ALS_OnDialogResponse
    #undef OnDialogResponse
#else
    #define _ALS_OnDialogResponse
#endif
#define OnDialogResponse Mb_OnDialogResponse
#if defined Mb_OnDialogResponse
    forward Mb_OnDialogResponse(playerid, dialogid, response, listitem, inputtext);
#endif

// OnPlayerClickTextDraw
#if defined _ALS_OnPlayerClickTextDraw
    #undef OnPlayerClickTextDraw
#else
    #define _ALS_OnPlayerClickTextDraw
#endif
#define OnPlayerClickTextDraw Mb_OnPlayerClickTextDraw
#if defined Mb_OnPlayerClickTextDraw
    forward Mb_OnPlayerClickTextDraw(playerid, Text:clickedid);
#endif

// OnPlayerClickPlayerTextDraw
#if defined _ALS_OnPlayerClickPlayerTD
    #undef OnPlayerClickPlayerTextDraw
#else
    #define _ALS_OnPlayerClickPlayerTD
#endif
#define OnPlayerClickPlayerTextDraw Mb_OnPlayerClickPlayerTD
#if defined Mb_OnPlayerClickPlayerTD
    forward Mb_OnPlayerClickPlayerTD(playerid, PlayerText:playertextid);
#endif