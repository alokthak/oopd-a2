#include "bookmgmt/Catalog.h"
#include "bookmgmt/Book.h"
#include "bookmgmt/EBook.h"
#include "bookmgmt/Journal.h"
#include <limits>

#include <algorithm>
#include <cctype>
#include <stdexcept>

#include "bookmgmt/Exceptions.h"

namespace bookmgmt {

namespace {
std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return s;
}
}  // namespace

Resource& Catalog::add(std::unique_ptr<Resource> r) {
    if (!r) throw std::invalid_argument("cannot add a null resource");
    const std::string id = r->id();
    if (items_.count(id)) throw DuplicateIdError(id);
    Entry& e = items_[id];
    e.resource = std::move(r);
    return *e.resource;
}

bool Catalog::contains(const std::string& id) const { return items_.count(id) > 0; }

Resource* Catalog::find(const std::string& id) {
    auto it = items_.find(id);
    return it == items_.end() ? nullptr : it->second.resource.get();
}

const Resource* Catalog::find(const std::string& id) const {
    auto it = items_.find(id);
    return it == items_.end() ? nullptr : it->second.resource.get();
}

Resource& Catalog::get(const std::string& id) {
    if (Resource* r = find(id)) return *r;
    throw NotFoundError(id);
}

const Resource& Catalog::get(const std::string& id) const {
    if (const Resource* r = find(id)) return *r;
    throw NotFoundError(id);
}

void Catalog::remove(const std::string& id) {
    get(id);
    if (!items_.at(id).borrowers.empty() || !items_.at(id).sessions.empty())
        throw LendingError("cannot remove a resource currently in use");
    items_.erase(id);
}

void Catalog::setVendorPrice(const std::string& id, const std::string& vendor, Money unitPrice) {
    get(id); // Validate the resource before accessing its entry.
    if (vendor.empty() || unitPrice.isNegative())
        throw std::invalid_argument("vendor name must be non-empty and price non-negative");
    auto& offers = items_.at(id).vendors;
    for (auto& offer : offers) {
        if (offer.vendor == vendor) {
            offer.unitPrice = unitPrice;
            return;
        }
    }
    offers.push_back({vendor, unitPrice});
}

const std::vector<VendorOffer>& Catalog::vendors(const std::string& id) const {
    get(id);
    return items_.at(id).vendors;
}

int Catalog::holdings(const std::string& id) const {
    auto it = items_.find(id);
    if (it == items_.end()) throw NotFoundError(id);
    return it->second.holdings;
}

void Catalog::addHoldings(const std::string& id, int units) {
    auto it = items_.find(id);
    if (it == items_.end()) throw NotFoundError(id);
    const auto next = static_cast<std::int64_t>(it->second.holdings) + units;
    if (next < 0) throw std::invalid_argument("holdings cannot become negative");
    if (next > std::numeric_limits<int>::max()) throw std::overflow_error("holdings are too large");
    const auto inUse = it->second.borrowers.size() + it->second.sessions.size();
    if (next < static_cast<std::int64_t>(inUse))
        throw LendingError("cannot reduce holdings below copies/seats in use");
    it->second.holdings = static_cast<int>(next);
}

int Catalog::available(const std::string& id) const {
    get(id);
    const auto& entry = items_.at(id);
    return entry.holdings - static_cast<int>(entry.borrowers.size() + entry.sessions.size());
}

const std::set<std::string>& Catalog::borrowers(const std::string& id) const {
    get(id);
    return items_.at(id).borrowers;
}

const std::set<std::string>& Catalog::sessionUsers(const std::string& id) const {
    get(id);
    return items_.at(id).sessions;
}

std::set<std::string>& Catalog::lendingUsers(const std::string& id, bool digital) {
    if (get(id).isDigital() != digital)
        throw LendingError(digital ? "sessions require an electronic resource" :
                                     "borrowing requires a print resource");
    return digital ? items_.at(id).sessions : items_.at(id).borrowers;
}

void Catalog::startUse(const std::string& id, const std::string& patron, bool digital) {
    auto& users = lendingUsers(id, digital);
    if (patron.empty()) throw std::invalid_argument("patron id must not be empty");
    if (users.count(patron)) throw LendingError("patron already uses this resource");
    if (available(id) == 0) throw LendingError("no copy/seat available");
    users.insert(patron);
}

void Catalog::endUse(const std::string& id, const std::string& patron, bool digital) {
    auto& users = lendingUsers(id, digital);
    if (patron.empty()) throw std::invalid_argument("patron id must not be empty");
    if (!users.erase(patron)) throw LendingError("patron does not use this resource");
}

void Catalog::borrow(const std::string& id, const std::string& patron) { startUse(id, patron, false); }
void Catalog::returnCopy(const std::string& id, const std::string& patron) { endUse(id, patron, false); }
void Catalog::openSession(const std::string& id, const std::string& patron) { startUse(id, patron, true); }
void Catalog::closeSession(const std::string& id, const std::string& patron) { endUse(id, patron, true); }

std::vector<const Resource*> Catalog::searchAuthor(const std::string& text) const {
    const std::string needle = lower(text);
    return where([&needle](const Resource& r) {
        const std::vector<std::string>* authors = nullptr;
        if (const auto* book = dynamic_cast<const Book*>(&r)) authors = &book->authors();
        else if (const auto* ebook = dynamic_cast<const EBook*>(&r)) authors = &ebook->authors();
        return authors && std::any_of(authors->begin(), authors->end(),
            [&needle](const std::string& author) { return lower(author).find(needle) != std::string::npos; });
    });
}

std::vector<const Resource*> Catalog::searchIdentifier(const std::string& identifier) const {
    if (identifier.empty()) return {};
    return where([&identifier](const Resource& r) {
        if (const auto* book = dynamic_cast<const Book*>(&r)) return book->isbn() == identifier;
        if (const auto* ebook = dynamic_cast<const EBook*>(&r)) return ebook->isbn() == identifier;
        if (const auto* journal = dynamic_cast<const Journal*>(&r)) return journal->issn() == identifier;
        return false;
    });
}

std::vector<const Resource*> Catalog::byYearRange(int firstYear, int lastYear) const {
    if (firstYear > lastYear) throw std::invalid_argument("year range is reversed");
    return where([firstYear, lastYear](const Resource& r) {
        return r.year() >= firstYear && r.year() <= lastYear;
    });
}

std::vector<const Resource*> Catalog::where(
    const std::function<bool(const Resource&)>& pred) const {
    std::vector<const Resource*> out;
    for (const auto& [id, entry] : items_)
        if (pred(*entry.resource)) out.push_back(entry.resource.get());
    return out;
}

std::vector<const Resource*> Catalog::all() const {
    return where([](const Resource&) { return true; });
}

std::vector<const Resource*> Catalog::byCategory(ResourceCategory c) const {
    return where([c](const Resource& r) { return r.category() == c; });
}

std::vector<const Resource*> Catalog::searchTitle(const std::string& text) const {
    const std::string needle = lower(text);
    return where([&needle](const Resource& r) {
        return lower(r.title()).find(needle) != std::string::npos;
    });
}

}  // namespace bookmgmt
