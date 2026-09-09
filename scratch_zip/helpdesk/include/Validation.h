#ifndef VALIDATION_H
#define VALIDATION_H

#include <string>

// -----------------------------------------------------------------------
// Validation
// -----------------------------------------------------------------------
// Pure, side-effect-free validation utilities with no database or I/O
// dependency, so they can be exercised directly in unit tests. They are
// used both by the console layer (fail fast, friendly UX) and by
// DatabaseManager (defense-in-depth: format is re-checked before any
// value reaches a SQL statement, per the requirement not to rely solely
// on UI-level validation).
// -----------------------------------------------------------------------
namespace Validation {

// Allowed characters: letters, digits, '.' and '_'. Length 3-32.
// (Case-insensitive uniqueness is enforced separately, at the database
// layer, since "uniqueness" is a cross-record concern this stateless
// utility cannot answer on its own.)
bool isValidUsernameFormat(const std::string& username);

// Minimal-but-correct structural check: exactly one '@', non-empty
// local and domain parts, domain contains at least one '.', no spaces,
// no empty labels (no leading/trailing/consecutive dots), and no
// leading/trailing whitespace anywhere in the string.
bool isValidEmailFormat(const std::string& email);

// Throwing variants used by call sites that want a ready-to-display
// error message on failure (throws ValidationException).
void validateUsernameOrThrow(const std::string& username);
void validateEmailOrThrow(const std::string& email);

} // namespace Validation

#endif // VALIDATION_H
