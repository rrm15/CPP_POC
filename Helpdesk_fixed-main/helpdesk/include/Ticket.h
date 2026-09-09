#ifndef TICKET_H
#define TICKET_H

#include <string>
#include "Common.h"

// -----------------------------------------------------------------------
// Ticket
// -----------------------------------------------------------------------
// Represents a single IT support ticket. All fields are encapsulated
// (private) and accessed/mutated only through getters and behavior
// methods (assignTo, updateStatus, resolve, addFeedback) so that the
// class always represents a valid, consistent state.
// -----------------------------------------------------------------------
class Ticket {
private:
    int id;                        // Primary key (assigned by the database, -1 if not yet persisted)
    int employeeId;                // ID of the User (Employee) who raised the ticket
    int assignedEngineerId;        // ID of the User (Engineer) assigned, -1 if unassigned
    std::string title;
    std::string description;
    TicketType type;
    TicketPriority priority;
    TicketStatus status;
    std::string createdAt;         // ISO-like timestamp string
    std::string updatedAt;
    std::string resolutionNotes;
    int feedbackRating;            // 0 = no feedback yet, else 1-5
    std::string feedbackComment;

public:
    // Constructor used when creating a brand-new ticket (not yet in DB)
    Ticket(int employeeId,
           const std::string& title,
           const std::string& description,
           TicketType type,
           TicketPriority priority);

    // Constructor used to reconstruct a ticket loaded from the database
    Ticket(int id, int employeeId, int assignedEngineerId,
           const std::string& title, const std::string& description,
           TicketType type, TicketPriority priority, TicketStatus status,
           const std::string& createdAt, const std::string& updatedAt,
           const std::string& resolutionNotes,
           int feedbackRating, const std::string& feedbackComment);

    // --- Getters (encapsulation: read-only external access) ---
    int getId() const { return id; }
    int getEmployeeId() const { return employeeId; }
    int getAssignedEngineerId() const { return assignedEngineerId; }
    const std::string& getTitle() const { return title; }
    const std::string& getDescription() const { return description; }
    TicketType getType() const { return type; }
    TicketPriority getPriority() const { return priority; }
    TicketStatus getStatus() const { return status; }
    const std::string& getCreatedAt() const { return createdAt; }
    const std::string& getUpdatedAt() const { return updatedAt; }
    const std::string& getResolutionNotes() const { return resolutionNotes; }
    int getFeedbackRating() const { return feedbackRating; }
    const std::string& getFeedbackComment() const { return feedbackComment; }

    // --- Setters needed for reconstruction / persistence sync ---
    void setId(int newId) { id = newId; }
    void setUpdatedAt(const std::string& ts) { updatedAt = ts; }

    // --- Behavior (business logic lives inside the class = encapsulation) ---
    void assignTo(int engineerId);                 // Assign / re-assign an engineer
    void updateStatus(TicketStatus newStatus);      // Move ticket through its lifecycle
    void resolve(const std::string& notes);         // Mark resolved + store notes
    void addFeedback(int rating, const std::string& comment); // Employee feedback

    // Prints a formatted, human-readable summary of the ticket to stdout.
    // Declared virtual to allow specialized display formatting via
    // polymorphism if the hierarchy is extended in the future
    // (e.g., a CriticalTicket subclass that prints extra warnings).
    virtual void display() const;

    virtual ~Ticket() = default;
};

#endif // TICKET_H
