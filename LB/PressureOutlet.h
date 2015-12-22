#pragma once
#include "D:\STUDIA\MEIL_MGR\LBM\mojeLB\LB\headers\Node.h"
#include  "Enums.h"

class PressureOutlet :
	public Node
{
private:
	CardinalDirections cardinal_directions;

public:
	void ComputeRho() override;
	void ComputeU() override;
	void NodeCollision(double const & omega) override;

	PressureOutlet() : cardinal_directions(West)
	{
		nodeType = NodeType::PressureOutletType;
	}; 
	~PressureOutlet();
};
