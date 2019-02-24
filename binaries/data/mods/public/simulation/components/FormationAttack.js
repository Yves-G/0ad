function FormationAttack() {}

FormationAttack.prototype.Schema =
	"<element name='CanAttackAsFormation'>" +
		"<text/>" +
	"</element>";

FormationAttack.prototype.Init = function()
{
	this.canAttackAsFormation = this.template.CanAttackAsFormation == "true";
};

FormationAttack.prototype.CanAttackAsFormation = function()
{
	if (this.GetMemberAttackTypes().has("Ranged"))
		return true;

	return this.canAttackAsFormation;
};

// Only called when making formation entities selectable for debugging
FormationAttack.prototype.GetAttackTypes = function()
{
	return [];
};

FormationAttack.prototype.GetRange = function(target)
{
	var result = {"min": 0, "max": this.canAttackAsFormation ? -1 : 0};
	var cmpFormation = Engine.QueryInterface(this.entity, IID_Formation);
	if (!cmpFormation)
	{
		warn("FormationAttack component used on a non-formation entity");
		return result;
	}
	var members = cmpFormation.GetMembers();
	for (var ent of members)
	{
		var cmpAttack = Engine.QueryInterface(ent, IID_Attack);
		if (!cmpAttack)
			continue;

		var type = cmpAttack.GetBestAttackAgainst(target);
		if (!type)
			continue;

		// if the formation can attack, take the minimum max range (so units are certainly in range),
		// If the formation can't attack, take the maximum max range as the point where the formation will be disbanded
		// Always take the minimum min range (to not get impossible situations)
		var range = cmpAttack.GetRange(type);

		if (this.canAttackAsFormation)
		{
			if (range.max < result.max || result.max < 0)
				result.max = range.max;
		}
		else
		{
			if (range.max > result.max || range.max < 0)
				result.max = range.max;
		}
		if (range.min < result.min)
			result.min = range.min;
	}
	// add half the formation size, so it counts as the range for the units on the first row
	var extraRange = cmpFormation.GetSize().depth/2;

	if (result.max >= 0)
		result.max += extraRange;

	return result;
};

FormationAttack.prototype.GetMemberAttackTypes = function()
{
	let ret = new Set();

	let cmpTemplateManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_TemplateManager);

	let cmpFormation = Engine.QueryInterface(this.entity, IID_Formation);
	if (!cmpFormation)
	{
		error("No Formation component!");
		return undefined;
	}

	for (let member of cmpFormation.GetMembers())
	{
		let cmpAttackMember = Engine.QueryInterface(member, IID_Attack);

		if (!cmpAttackMember)
			continue;

		let attackTypesMember = cmpAttackMember.GetAttackTypes();
		for (let attackTypeMember of attackTypesMember)
			ret.add(attackTypeMember)
	}

	return ret;
};

/**
 * Returns null if we have no preference or the lowest index of a preferred class.
 */
FormationAttack.prototype.GetPreference = function(target)
{
	let cmpFormation = Engine.QueryInterface(this.entity, IID_Formation);
	if (!cmpFormation)
	{
		error("No Formation component!");
		return undefined;
	}

	let cmpIdentity = Engine.QueryInterface(target, IID_Identity);
	if (!cmpIdentity)
		return undefined;

	let cmpTemplateManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_TemplateManager);
	let targetClasses = cmpIdentity.GetClassesList();

	// First, get all unit types in the formation
	// This is dynamic because units can die, and all units of one type could have died
	let unitTypes = new Set();
	let attackTypes = {};
	for (let member of cmpFormation.GetMembers())
	{
		let cmpAttackMember = Engine.QueryInterface(member, IID_Attack);

		if (!cmpAttackMember)
			continue;

		// Only check once per template (unit type)
		let templateNameMember = cmpTemplateManager.GetCurrentTemplateName(member);
		if (unitTypes.has(templateNameMember))
			continue;

		unitTypes.add(templateNameMember);

		// When ranged and melee troops are combined in a battalion, we prefer ranged.
		// When different unit types with the same attack type (ranged, melee) are combined,
		// the preference is arbitrarily chosen from one of the templates.
		// The idea is that battalions/formations should be one unit type in general or
		// or a mix of two (siege + melee, archers + melee)

		let attackTypesMember = cmpAttackMember.GetAttackTypes();
		for (let attackTypeMember of attackTypesMember)
		{
			if (!!attackTypes[attackTypeMember])
				continue;

			let preferredClasses = cmpAttackMember.GetPreferredClasses(attackTypeMember);
			let minPref = null;
			for (let targetClass of targetClasses)
			{
				let pref = preferredClasses.indexOf(targetClass);
				if (pref === 0)
				{
					minPref = pref;
					break;
				}
				if (pref != -1 && (minPref === null || minPref > pref))
					minPref = pref;
			}
			attackTypes[attackTypeMember] = minPref;

			if (attackTypeMember == "Ranged")
				break;
		}

		// Choose ranged attack, if available
		if (!!attackTypes["Ranged"])
			return attackTypes["Ranged"];
	}

	// If no ranged attack, choose the highest priority of the other available attack types
	let minPref = null;
	for (let attackType in attackTypes)
	{
		let pref = attackTypes[attackType];
		if (minPref === null || minPref > pref)
			minPref = pref;
	}
	return minPref
};
	/*let cmpIdentity = Engine.QueryInterface(target, IID_Identity);
	if (!cmpIdentity)
		return undefined;

	let targetClasses = cmpIdentity.GetClassesList();

	let minPref = null;
	for (let type of this.GetAttackTypes())
	{
		let preferredClasses = this.GetPreferredClasses(type);
		for (let targetClass of targetClasses)
		{
			let pref = preferredClasses.indexOf(targetClass);
			if (pref === 0)
				return pref;
			if (pref != -1 && (minPref === null || minPref > pref))
				minPref = pref;
		}
	}
	return minPref;*/

/*
FormationAttack.prototype.GetPreferredClasses = function(type)
{
	if (this.template[type] && this.template[type].PreferredClasses &&
	    this.template[type].PreferredClasses._string)
		return this.template[type].PreferredClasses._string.split(/\s+/);

	return [];
};*/


Engine.RegisterComponentType(IID_Attack, "FormationAttack", FormationAttack);
