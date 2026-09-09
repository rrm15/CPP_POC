#include "Ticket.h"
#include "DateUtil.h"
#include <iostream>
#include <iomanip>

// New ticket constructor: status always starts as OPEN, unassigned.
Ticket::Ticket(int employeeId,
               const std::string& title,
               const std::string& description,
               TicketType type,
               TicketPriority priority)
    : id(-1),
      employeeId(employeeId),
      assignedEngineerId(-1),
      title(title),
      description(description),
      type(type),
      priority(priority),
      status(TicketStatus::OPEN),
      createdAt(DateUtil::nowString()),
      updatedAt(DateUtil::nowString()),
      resolutionNotes(""),
      feedbackRating(0),
      feedbackComment("")
{
    if (title.empty()) {
        throw ValidationException("Ticket title cannot be empty.");
    }
}

// Reconstruction constructor (used when loading rows from SQLite).
Ticket::Ticket(int id, int employeeId, int assignedEngineerId,
               const std::string& title, const std::string& description,
               TicketType type, TicketPriority priority, TicketStatus status,
               const std::string& createdAt, const std::string& updatedAt,
               const std::string& resolutionNotes,
               int feedbackRating, const std::string& feedbackComment)
    : id(id),
      employeeId(employeeId),
      assignedEngineerId(assignedEngineerId),
      title(title),
      description(description),
      type(type),
      priority(priority),
      status(status),
      createdAt(createdAt),
      updatedAt(updatedAt),
      resolutionNotes(resolutionNotes),
      feedbackRating(feedbackRating),
      feedbackComment(feedbackComment)
{}

void Ticket::assignTo(int engineerId) {
    if (engineerId <= 0) {
        throw ValidationException("Invalid engineer ID for assignment.");
    }
    if (status == TicketStatus::RESOLVED || status == TicketStatus::CLOSED) {
        throw ValidationException("Cannot assign a ticket that is already resolved/closed.");
    }
    assignedEngineerId = engineerId;
    status = TicketStatus::ASSIGNED;
    updatedAt = DateUtil::nowString();
}

void Ticket::updateStatus(TicketStatus newStatus) {
    if (status == TicketStatus::CLOSED) {
        throw ValidationException("Cannot change status of a CLOSED ticket.");
    }
    status = newStatus;
    updatedAt = DateUtil::nowString();
}

void Ticket::resolve(const std::string& notes) {
    if (assignedEngineerId == -1) {
        throw ValidationException("Cannot resolve a ticket with no assigned engineer.");
    }
    if (notes.empty()) {
        throw ValidationException("Resolution notes cannot be empty.");
    }
    resolutionNotes = notes;
    status = TicketStatus::RESOLVED;
    updatedAt = DateUtil::nowString();
}

void Ticket::addFeedback(int rating, const std::string& comment) {
    if (status != TicketStatus::RESOLVED && status != TicketStatus::CLOSED) {
        throw ValidationException("Feedback can only be given for resolved/closed tickets.");
    }
    if (rating < 1 || rating > 5) {
        throw ValidationException("Feedback rating must be between 1 and 5.");
    }
    feedbackRating = rating;
    feedbackComment = comment;
    updatedAt = DateUtil::nowString();
}

void Ticket::display() const {
    std::cout << "--------------------------------------------------------\n";
    std::cout << "Ticket #" << id << " | " << title << "\n";
    std::cout << "  Type     : " << ticketTypeToLabel(type) << "\n";
    std::cout << "  Priority : " << priorityToString(priority) << "\n";
    std::cout << "  Status   : " << statusToString(status) << "\n";
    std::cout << "  Raised by (Employee ID): " << employeeId << "\n";
    std::cout << "  Assigned Engineer ID   : "
              << (assignedEngineerId == -1 ? std::string("Unassigned") : std::to_string(assignedEngineerId))
              << "\n";
    std::cout << "  Description : " << description << "\n";
    std::cout << "  Created At  : " << createdAt << "\n";
    std::cout << "  Updated At  : " << updatedAt << "\n";
    if (!resolutionNotes.empty()) {
        std::cout << "  Resolution  : " << resolutionNotes << "\n";
    }
    if (feedbackRating > 0) {
        std::cout << "  Feedback    : " << feedbackRating << "/5 - " << feedbackComment << "\n";
    }
    std::cout << "--------------------------------------------------------\n";
}
