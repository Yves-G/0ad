function init()
{
	var languageList = Engine.GetGUIObjectByName("languageList");
	languageList.list = Engine.GetSupportedLocaleDisplayNames();
	languageList.list_data = Engine.GetSupportedLocaleCodes();
	languageList.selected = Engine.GetCurrentLocaleIndex();
}

function cancelSetup()
{
	Engine.PopGuiPage();
}

function applySelectedLocale()
{
	var languageList = Engine.GetGUIObjectByName("languageList");
	Engine.SetLocale(languageList.list_data[languageList.selected]);
	Engine.SwitchGuiPage("page_pregame.xml");
}
