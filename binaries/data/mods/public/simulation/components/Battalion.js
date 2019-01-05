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
	"</element>";

Battalion.prototype.Init = function()
{
	this.numberOfUnits = this.template.NumberOfUnits;
	this.spawnFormationTemplate = this.template.SpawnFormationTemplate;
	this.templateName = this.template.TemplateName;
	this.entities = [];
	this.formationEntity = INVALID_ENTITY;

	warn("Init. entities.length: " + this.entities.length);

	//this.SpawnUnits();
};


/**
 * Spawn all member units of the formation.
 */
Battalion.prototype.SpawnUnits = function(playerId)
{
	//var template = Engine.QueryInterface(SYSTEM_ENTITY, IID_TemplateManager).GetTemplate(this.templateName);
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

		cmpNewOwnership.SetOwner(playerId)

		let cmpUnitAI = Engine.QueryInterface(ent, IID_UnitAI);
		cmpUnitAI.SetStance("defensive");

		this.entities.push(ent);
	}

	this.CreateFormation();
};

Battalion.prototype.CreateFormation = function()
{
	if (this.formationEntity === INVALID_ENTITY)
	{
		// Create the new controller
		warn("create formation");
		this.formationEntity = Engine.AddEntity(this.spawnFormationTemplate);
	}

	let cmpFormation = Engine.QueryInterface(this.formationEntity, IID_Formation);
	cmpFormation.SetMembers(this.entities);
	cmpFormation.AddMembers([this.entity]);

	for (let entity of this.entities)
	{
		let cmpBattalionMember = Engine.QueryInterface(entity, IID_BattalionMember);
		cmpBattalionMember.SetLeader(this.entity);
	}
}

Battalion.prototype.GetMembers = function()
{
	return this.entities;
}

Battalion.prototype.SetMembers = function(entities)
{
	this.entities = entities;
	warn("Battalion.SetMembers");
}

Battalion.prototype.SetFormationEntity = function(formationEnt)
{
	this.formationEntity = formationEnt;
}

Battalion.prototype.GetFormationEntity = function()
{
	return this.formationEntity;
}

Battalion.prototype.GetLeader = function()
{
	return this.entity;
}

Battalion.prototype.RemoveMember = function(ent)
{
	let index = this.entities.indexOf(ent);
	if (index !== -1) this.entities.splice(index, 1);

	// The leader is invincible as long as there are other units in the battalion
	if (this.entities.length == 0)
	{
		let cmpHealth = Engine.QueryInterface(this.entity, IID_Health);
		cmpHealth.SetInvincible(false);
	}
}

Battalion.prototype.OnOwnershipChanged = function(msg)
{
	if (msg.from == INVALID_PLAYER)
	{
		if (!this.entities.length)
		{
			warn("spawnUnits");
			this.SpawnUnits(msg.to);
		}
	}
}


Engine.RegisterComponentType(IID_Battalion, "Battalion", Battalion);
