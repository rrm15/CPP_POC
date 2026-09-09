#include "../include/Ticket.h"
#include "../include/DatabaseManager.h"
#include <chrono>

TicketManager::TicketManager() {}

TicketManager::~TicketManager() {}

int TicketManager::createTicket(int customer_id, const std::string& title,
                               const std::string& description) {
    auto& db = DatabaseManager::getInstance();
    return db.insertTicket(customer_id, title, description);
}

bool TicketManager::assignTicket(int ticket_id, int engineer_id) {
    auto& db = DatabaseManager::getInstance();
    return db.conditionalAssignTicket(ticket_id, engineer_id);
}

bool TicketManager::updateTicketStatus(int ticket_id, TicketStatus new_status) {
    auto& db = DatabaseManager::getInstance();
    return db.conditionalUpdateStatus(ticket_id, new_status);
}

bool TicketManager::resolveTicket(int ticket_id, const std::string& resolution) {
    auto& db = DatabaseManager::getInstance();
    return db.conditionalResolveTicket(ticket_id, resolution);
}

std::vector<Ticket> TicketManager::getTicketsByStatus(TicketStatus status) {
    auto& db = DatabaseManager::getInstance();
    return db.getTicketsByStatus(status);
}

std::vector<Ticket> TicketManager::getTicketsByCustomer(int customer_id) {
    // Implementation would query database
    std::vector<Ticket> result;
    return result;
}

std::vector<Ticket> TicketManager::getTicketsByEngineer(int engineer_id) {
    // Implementation would query database
    std::vector<Ticket> result;
    return result;
}

Ticket* TicketManager::getTicketById(int ticket_id) {
    auto& db = DatabaseManager::getInstance();
    return db.getTicketById(ticket_id);
}
