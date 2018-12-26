function Battalion() {}

Battalion.prototype.Schema =
	"<a:help>Controls the damage resistance of the unit.</a:help>" +
	"<element name='NumberOfUnits' a:help='Number of entities the battalion should contain'>" +
		"<data type='nonNegativeInteger'/>" +
	"</element>" +
	"<element name='SpawnFormationTemplate' a:help='Default formation template to use when the battalion spawns.'>" +
		"<text/>" +
	"</element>" +
	"<element name='TemplateName' a:help='Entity template name of the units in this battalion. The special string \"{civ}\" will be automatically replaced by the civ code of the entity&apos;s owner, while the string \"{native}\" will be automatically replaced by the entity&apos;s civ code.'>" +
		"<text/>" +
	"</element>" +
	"<element name='LeaderTemplateName' a:help='Entity template name of the unit leading the battalion. The special string \"{civ}\" will be automatically replaced by the civ code of the entity&apos;s owner, while the string \"{native}\" will be automatically replaced by the entity&apos;s civ code.'>" +
		"<text/>" +
	"</element>";

Battalion.prototype.Init = function()
{
	this.numberOfUnits = this.template.NumberOfUnits;
	this.spawnFormationTemplate = this.template.SpawnFormationTemplate;
	this.templateName = this.template.TemplateName;
	this.leaderTemplateName = this.template.LeaderTemplateName
	this.entities = [];

	//this.SpawnUnits();
};


/**
 * Take damage according to the entity's armor.
 * @param {Object} strengths - { "hack": number, "pierce": number, "crush": number } or something like that.
 * @param {number} multiplier - the damage multiplier.
 * Returns object of the form { "killed": false, "change": -12 }.
 */
Battalion.prototype.SpawnUnits = function()
{
	//var template = Engine.QueryInterface(SYSTEM_ENTITY, IID_TemplateManager).GetTemplate(this.templateName);
	var cmpOwnership = Engine.QueryInterface(this.entity, IID_Ownership);
	var cmpFootprint = Engine.QueryInterface(this.entity, IID_Footprint);

	for (let i = 0; i < this.numberOfUnits; ++i)
	{
		var ent = Engine.AddEntity(this.templateName);
		let cmpNewOwnership = Engine.QueryInterface(ent, IID_Ownership);

		let pos = cmpFootprint.PickSpawnPoint(ent);
		if (pos.y < 0)
			break;

		let cmpNewPosition = Engine.QueryInterface(ent, IID_Position);
		cmpNewPosition.JumpTo(pos.x, pos.z);

		let cmpPosition = Engine.QueryInterface(this.entity, IID_Position);
		if (cmpPosition)
			cmpNewPosition.SetYRotation(cmpPosition.GetPosition().horizAngleTo(pos));

		cmpNewOwnership.SetOwner(cmpOwnership.GetOwner())

		let cmpNewBattalionMember = Engine.QueryInterface(ent, IID_BattalionMember);
		cmpNewBattalionMember.SetLeader(this.entity);

		this.entities.push(ent);
	}

	this.CreateFormation();
};

Battalion.prototype.CreateFormation = function()
{
	// Create the new controller
	var formationEnt = Engine.AddEntity(this.spawnFormationTemplate);
	var cmpFormation = Engine.QueryInterface(formationEnt, IID_Formation);

	cmpFormation.SetMembers(this.entities);
	
	var cmpOwnership= Engine.QueryInterface(this.entity, IID_Ownership);
	var newCmpOwnership= Engine.QueryInterface(formationEnt, IID_Ownership);
	cmpOwnership.SetOwner(cmpOwnership.GetOwner());

	cmpFormation.AddMembers([this.entity]);
}

Battalion.prototype.GetMembers = function()
{
	return this.entities;
}

Battalion.prototype.GetLeader = function()
{
	return this.entity;
}


Engine.RegisterComponentType(IID_Battalion, "Battalion", Battalion);
