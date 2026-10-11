#include "bookmgmt/Journal.h"
#include <ostream>
#include <stdexcept>
#include <utility>

namespace bookmgmt {
Journal::Journal(std::string id, std::string title, std::string issn, int issuesPerYear,
                 std::string publisher, int year, Money annualPrice, int subscriptionYears)
    : Resource(std::move(id), std::move(title), std::move(publisher), year, annualPrice),
      issn_(std::move(issn)), issuesPerYear_(issuesPerYear), subscriptionYears_(subscriptionYears) {
    if (subscriptionYears_ < 1)
        throw std::invalid_argument("subscription years must be >= 1");
    if (issuesPerYear_ < 1)
        throw std::invalid_argument("issues per year must be >= 1");
}
Money Journal::costFor(int copies) const {
    requirePositive(copies);
    return printPrice(unitPrice() * copies * subscriptionYears_, copies);
}
void Journal::printDetails(std::ostream& os) const {
    os << "  issn: " << issn_ << "\n"
       << "  issues per year: " << issuesPerYear_ << "\n"
       << "  subscription years: " << subscriptionYears_ << "\n";
}
}  // namespace bookmgmt
