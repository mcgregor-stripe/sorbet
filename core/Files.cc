#include "core/Files.h"
#include "core/Context.h"
#include "core/GlobalState.h"
#include <vector>

#include "absl/strings/match.h"

template class std::vector<std::shared_ptr<sorbet::core::File>>;
template class std::shared_ptr<sorbet::core::File>;
using namespace std;

namespace sorbet {
namespace core {

vector<int> findLineBreaks(const string &s) {
    vector<int> res;
    int i = -1;
    res.push_back(-1);
    for (auto c : s) {
        i++;
        if (c == '\n') {
            res.push_back(i);
        }
    }
    res.push_back(i);
    return res;
}

StrictLevel fileSigil(string_view source) {
    /*
     * StrictLevel::Stripe: <none> | # typed: false
     * StrictLevel::Typed: # typed: true
     * StrictLevel::Strict: # typed: strict
     * StrictLevel::String: # typed: strong
     * StrictLevel::Autogenerated: # typed: autogenerated
     */
    size_t start = 0;
    while (true) {
        start = source.find("typed:", start);
        if (start == string_view::npos) {
            return StrictLevel::Stripe;
        }
        start += 6;
        while (start < source.size() && source[start] == ' ') {
            ++start;
        }

        if (start >= source.size()) {
            return StrictLevel::Stripe;
        }
        auto end = start + 1;
        while (end < source.size() && source[end] != ' ' && source[end] != '\n') {
            ++end;
        }

        string_view suffix = source.substr(start, end - start);
        if (suffix == "false") {
            return StrictLevel::Stripe;
        } else if (suffix == "true") {
            return StrictLevel::Typed;
        } else if (suffix == "strict") {
            return StrictLevel::Strict;
        } else if (suffix == "strong") {
            return StrictLevel::Strong;
        } else if (suffix == "autogenerated") {
            return StrictLevel::Autogenerated;
        } else {
            // TODO(nelhage): We should report an error here to help catch
            // typos. This would require refactoring so this function has
            // access to GlobalState or can return errors to someone who
            // does.
        }

        start = end;
    }
}

File::File(string &&path_, string &&source_, Type sourceType)
    : sourceType(sourceType), path_(path_), source_(source_), sigil(fileSigil(this->source_)), strict(sigil) {}

FileRef::FileRef(unsigned int id) : _id(id) {
    ENFORCE(((u2)id) == id, "FileRef overflow. Do you have 2^16 files?");
}

const File &FileRef::data(const GlobalState &gs, bool allowTombStones) const {
    ENFORCE(_id < gs.filesUsed());
    ENFORCE(allowTombStones || gs.files[_id]->sourceType != File::TombStone);
    return *(gs.files[_id]);
}

File &FileRef::data(GlobalState &gs, bool allowTombStones) const {
    ENFORCE(_id < gs.filesUsed());
    ENFORCE(allowTombStones || gs.files[_id]->sourceType != File::TombStone);
    return *(gs.files[_id]);
}

string_view File::path() const {
    return this->path_;
}

string_view File::source() const {
    ENFORCE(this->sourceType != Type::TombStone);
    return this->source_;
}

bool File::hadErrors() const {
    return hadErrors_;
}

bool File::isPayload() const {
    return sourceType == Type::PayloadGeneration || sourceType == Type::Payload;
}

bool File::isRBI() const {
    return absl::EndsWith(path(), ".rbi");
}

std::vector<int> &File::line_breaks() const {
    ENFORCE(this->sourceType != Type::TombStone);
    auto ptr = atomic_load(&line_breaks_);
    if (ptr) {
        return *ptr;
    } else {
        auto my = make_shared<vector<int>>(findLineBreaks(this->source_));
        atomic_compare_exchange_weak(&line_breaks_, &ptr, my);
        return line_breaks();
    }
}

int File::lineCount() const {
    return line_breaks().size() - 1;
}

} // namespace core
} // namespace sorbet
