#pragma once
// Catalog: owns every Resource, keyed by its id, and tracks copies/seats held.

#include <functional>
#include <set>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "bookmgmt/Resource.h"

namespace bookmgmt {

struct VendorOffer {
    std::string vendor;
    Money unitPrice;
};

class Catalog {
public:
    // Takes ownership. Throws DuplicateIdError if the id is already present.
    Resource& add(std::unique_ptr<Resource> r);

    // Convenience: catalog.emplace<Book>(ctor args...)
    template <typename T, typename... Args>
    T& emplace(Args&&... args) {
        auto p = std::make_unique<T>(std::forward<Args>(args)...);
        T& ref = *p;
        add(std::move(p));
        return ref;
    }

    bool contains(const std::string& id) const;
    Resource* find(const std::string& id);              // nullptr if absent
    const Resource* find(const std::string& id) const;  // nullptr if absent
    Resource& get(const std::string& id);               // throws NotFoundError
    const Resource& get(const std::string& id) const;   // throws NotFoundError
    void remove(const std::string& id);                 // throws NotFoundError

    // Add or update a named vendor's per-copy/seat/annual-subscription price.
    void setVendorPrice(const std::string& id, const std::string& vendor, Money unitPrice);
    const std::vector<VendorOffer>& vendors(const std::string& id) const;

    std::size_t size() const { return items_.size(); }
    bool empty() const { return items_.empty(); }

    // Holdings: number of copies (print) or seats (electronic) owned.
    int holdings(const std::string& id) const;
    // Reductions may not consume copies/seats currently in use.
    void addHoldings(const std::string& id, int units);
    int available(const std::string& id) const;
    void borrow(const std::string& id, const std::string& patron);
    void returnCopy(const std::string& id, const std::string& patron);
    void openSession(const std::string& id, const std::string& patron);
    void closeSession(const std::string& id, const std::string& patron);
    const std::set<std::string>& borrowers(const std::string& id) const;
    const std::set<std::string>& sessionUsers(const std::string& id) const;

    // Queries. Results are in id order and point into the catalog.
    std::vector<const Resource*> all() const;
    std::vector<const Resource*> byCategory(ResourceCategory c) const;
    std::vector<const Resource*> searchTitle(const std::string& text) const;  // case-insensitive
    std::vector<const Resource*> searchAuthor(const std::string& text) const;
    std::vector<const Resource*> searchIdentifier(const std::string& isbnOrIssn) const;
    std::vector<const Resource*> byYearRange(int firstYear, int lastYear) const;

    std::vector<const Resource*> where(
        const std::function<bool(const Resource&)>& pred) const;

private:
    struct Entry {
        std::unique_ptr<Resource> resource;
        int holdings = 0;
        std::vector<VendorOffer> vendors;
        std::set<std::string> borrowers;
        std::set<std::string> sessions;
    };
    std::set<std::string>& lendingUsers(const std::string& id, bool digital);
    void startUse(const std::string& id, const std::string& patron, bool digital);
    void endUse(const std::string& id, const std::string& patron, bool digital);
    std::map<std::string, Entry> items_;
};

}  // namespace bookmgmt
