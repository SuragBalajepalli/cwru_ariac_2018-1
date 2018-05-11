#include "optimizer_func/optimizer_func.h"
#include "optimizer_func/helper_funcs.h"

int short_term_memory = 0;
int middle_term_memory = 0;

// Could make this a time based decision instead of being based on count
#define NUM_REP_REQS	8

bool optimize_shipments(optimizer_func::optimizer_msgs::Request  &req,
			optimizer_func::optimizer_msgs::Response &res)
{

  //  ROS_INFO("Q%i asking about %s.  Next in queue is %s.", req.loaded.shipment_type.c_str(), req.inspection_site, shipment_queue.shipments[1].shipment_type.c_str());

  // Do different things based on which station, Q1 or Q1, are querying.
  switch (req.inspection_site) {
  case optimizer_func::optimizer_msgsRequest::Q1_STATION:

    // Giving up or there were a lot of repeats without indication of moving forward
    if ((req.giving_up == optimizer_func::optimizer_msgsRequest::GIVING_UP) || (middle_term_memory > NUM_REP_REQS)) {

      ROS_INFO("Giving up set, so advance the box (next shipment returned).");

      // Set the decision to move forward as requested by the "give_up" flag.
      res.decision = optimizer_func::optimizer_msgsResponse::ADVANCE_THIS_BOX_TO_Q2;

      // Make sure that the shipment query is for the expected shipment.  Also accepting a NULL_SHIPMENT string
      if (shipment_queue.shipments[0].shipment_type == std::string(req.loaded.shipment_type)) {
	if (!req.loaded.shipment_type.compare(NULL_SHIPMENT)) {
	  ROS_WARN("The optimizer expected %s, but received %s instead", shipment_queue.shipments[0].shipment_type.c_str(), req.loaded.shipment_type.c_str());
	}
	// The last entry is a empty shipment queue placeholder, so don't pop it.
	if(shipment_queue.shipments.size() > 1) 
	  shipment_queue.shipments.erase(shipment_queue.shipments.begin());
      }	else
	ROS_WARN("Expecting shipment %s, and received shipment %s", shipment_queue.shipments[0].shipment_type.c_str(), req.loaded.shipment_type.c_str());

      // Reset short term memory.
      short_term_memory = 0;
      
    } else if (req.giving_up == optimizer_func::optimizer_msgsRequest::NOT_GIVING_UP) {
      ROS_INFO("Not giving up, so keep using the same box (same shipment returned).");

      // Short term memory keeps track of the state of the shipment process 
      // to prevent a no-progress state from continuing too long.
      if (short_term_memory == req.loaded.products.size() + req.orphaned.products.size() + req.missing.products.size() + req.reposition.products.size()) {
	// Increment a middle term memory which is checked to force a "give_up" above when reaching NUM_REP_REQS.
	ROS_WARN("middle_term_memory: %i", middle_term_memory);
	middle_term_memory++;
      } else {
	// Reset the middle term memory since the 
	middle_term_memory = 0;
      }

      // Set the short term memory.
      short_term_memory = req.loaded.products.size() + req.orphaned.products.size() + req.missing.products.size() + req.reposition.products.size();

      // This is where a decision is made about how to proceed.  Not a smart method yet.
      res.decision = optimizer_func::optimizer_msgsResponse::USE_CURRENT_BOX;
    }
    ROS_INFO("Sending next shipment; queue size: %i", (int) shipment_queue.shipments.size());

    break;

    // Section for Q2 queries
  case optimizer_func::optimizer_msgsRequest::Q2_STATION:

    // Giving up
    if (req.giving_up ==  optimizer_func::optimizer_msgsRequest::GIVING_UP) {
      ROS_INFO("Giving up.  Responding PRIORIYT_LOAD_NEXT and work on shipment %s", shipment_queue.shipments[0].shipment_type.c_str());
      res.decision = optimizer_func::optimizer_msgsResponse::PRIORITY_LOAD_NEXT;
    } else if (req.giving_up ==  optimizer_func::optimizer_msgsRequest::NOT_GIVING_UP) {

      // TODO:
      // The only thing being checked here is whether the overall score will be substanially
      // dropped by not removing a faulty part.

      // If there isn't anything missing, orphaned, or in need of repositioning, or if the alert_level is now red, send the box on its way.
      if (((req.missing.products.size() == 0) && (req.orphaned.products.size() == 0) && (req.reposition.products.size() == 0)) || (alert_level == LEVEL_RED)) {
	ROS_INFO("Not giving up, check to decide whether to do anything.");
	res.decision = optimizer_func::optimizer_msgsResponse::PRIORITY_LOAD_NEXT;
      } else {
	res.decision = optimizer_func::optimizer_msgsResponse::USE_CURRENT_BOX;
      }
    } else {
      ROS_ERROR("Not a known giving_up entry!! Just advancing shipment.");
      res.decision = optimizer_func::optimizer_msgsResponse::PRIORITY_LOAD_NEXT;
    }
    break;

  default:
    ROS_ERROR("Request did not come from Q1 or Q2!!");
    res.decision = optimizer_func::optimizer_msgsResponse::USE_CURRENT_BOX;
    break;
  }
  ROS_INFO("Sending information on %s", shipment_queue.shipments[0].shipment_type.c_str());
  res.shipment = shipment_queue.shipments[0];

  return true;
}

