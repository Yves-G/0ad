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

Engine.RegisterComponentType(IID_BattalionMember, "BattalionMember", BattalionMember);
