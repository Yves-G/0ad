function init(data)
{
	Engine.GetGUIObjectByName("mainText").caption = Engine.translateLines(Engine.ReadFile("gui/splashscreen/" + data.page + ".txt"));
}
