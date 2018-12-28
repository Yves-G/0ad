function BattalionMember() {}

BattalionMember.prototype.Schema = "<empty/>";

BattalionMember.prototype.Init = function()
{
	this.leader = INVALID_ENTITY;
};


/**
 * Get the entity id of the battalion leader
 * Returns object of the form { "killed": false, "change": -12 }.
 */
BattalionMember.prototype.GetLeader = function()
{
	return this.leader;
}

BattalionMember.prototype.SetLeader = function(ent)
{
	this.leader = ent;
}

BattalionMember.prototype.OnOwnershipChanged = function(msg)
{
	if (msg.from == INVALID_PLAYER)
		return;

	// When an entity is captured or destroyed, it should no longer be
	// controlled by this battalion
	let cmpBattalion = Engine.QueryInterface(this.leader, IID_Battalion);
	if (!cmpBattalion)
		return;

	cmpBattalion.RemoveMember(msg.entity);
}

Engine.RegisterComponentType(IID_BattalionMember, "BattalionMember", BattalionMember);
