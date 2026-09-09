#ifndef TICKET_H
#define TICKET_H

#include "Common.h"
#include <vector>
#include <memory>

class TicketManager {
public:
    TicketManager();
    ~TicketManager();
    
    // Ticket management methods
    int createTicket(int customer_id, const std::string& title, 
                    const std::string& description);
    bool assignTicket(int ticket_id, int engineer_id);
    bool updateTicketStatus(int ticket_id, TicketStatus new_status);
    bool resolveTicket(int ticket_id, const std::string& resolution);
    std::vector<Ticket> getTicketsByStatus(TicketStatus status);
    std::vector<Ticket> getTicketsByCustomer(int customer_id);
    std::vector<Ticket> getTicketsByEngineer(int engineer_id);
    Ticket* getTicketById(int ticket_id);
    
private:
    std::vector<std::shared_ptr<Ticket>> tickets;
};

#endif  // TICKET_H
