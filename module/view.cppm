export module kqm.str.view;

import std;
import kqm.str.utf;

export namespace kqm
{

struct string_view
{
public:
    using value_type             = char32_t;
    using size_type              = std::size_t;
    using difference_type        = std::ptrdiff_t;
    using const_iterator         = utf::utf8_to_utf32_iterator<const std::byte*>;
    using iterator               = const_iterator;
    using const_reverse_iterator = std::reverse_iterator<iterator>;
    using reverse_iterator       = const_reverse_iterator;

private:
    iterator first_;
    iterator last_;

public:
    string_view() noexcept : first_{}, last_{} {}

    string_view(iterator first, iterator last) noexcept : first_{first}, last_{last} {}

    string_view(std::string_view sv) noexcept
        : first_{reinterpret_cast<const std::byte*>(sv.begin())}, last_{reinterpret_cast<const std::byte*>(sv.end())}
    {
    }

    string_view(std::nullptr_t) = delete;


    constexpr auto begin() const noexcept -> iterator
    {
        return first_;
    }

    constexpr auto cbegin() const noexcept -> iterator
    {
        return begin();
    }

    constexpr auto end() const noexcept -> iterator
    {
        return last_;
    }

    constexpr auto cend() const noexcept -> iterator
    {
        return end();
    }

    constexpr auto rbegin() const noexcept -> const_reverse_iterator
    {
        return const_reverse_iterator{begin()};
    }

    constexpr auto crbegin() const noexcept -> const_reverse_iterator
    {
        return rbegin();
    }


    constexpr auto rend() const noexcept -> const_reverse_iterator
    {
        return const_reverse_iterator{end()};
    }

    constexpr auto crend() const noexcept -> const_reverse_iterator
    {
        return rend();
    }


    constexpr auto front() const noexcept -> value_type
    {
        return *begin();
    }

    constexpr auto back() const noexcept -> value_type
    {
        return *(std::prev(end()));
    }

    constexpr auto data() const noexcept -> const std::byte*
    {
        return first_.base();
    }

    constexpr auto as_bytes() const noexcept -> std::span<const std::byte>
    {
        return {first_.base(), last_.base()};
    }

    constexpr auto as_const_bytes() const noexcept -> std::span<const std::byte>
    {
        return {first_.base(), last_.base()};
    }


    constexpr auto size_bytes() const noexcept -> size_type
    {
        return last_.base() - first_.base();
    }

    static constexpr auto max_size_bytes() noexcept -> size_type
    {
        return std::numeric_limits<size_type>::max();
    }

    constexpr auto empty() const noexcept -> bool
    {
        return first_ == last_;
    }

    constexpr auto remove_prefix(std::size_t n) noexcept -> void
    {
        while (n--) {
            ++first_;
        }
    }

    constexpr auto remove_suffix(std::size_t n) noexcept -> void
    {
        while (n--) {
            --last_;
        }
    }

    constexpr auto swap(string_view& other) & noexcept -> void
    {
        std::swap(first_, other.first_);
        std::swap(last_, other.last_);
    }

    constexpr auto start_with(value_type ch) const noexcept -> bool
    {
        return !empty() && front() == ch;
    }

    constexpr auto start_with(string_view sv) const noexcept -> bool
    {
        return std::ranges::starts_with(as_bytes(), sv.as_bytes());
    }

    constexpr auto ends_with(value_type ch) const noexcept -> bool
    {
        return !empty() && back() == ch;
    }

    constexpr auto ends_with(string_view sv) const noexcept -> bool
    {
        return std::ranges::ends_with(as_bytes(), sv.as_bytes());
    }

    constexpr auto contains(value_type ch) const noexcept -> bool
    {
        return std::ranges::contains(*this, ch);
    }

    constexpr auto contains(string_view sv) const noexcept -> bool
    {
        return std::ranges::contains_subrange(as_bytes(), sv.as_bytes());
    }

    constexpr auto find(value_type ch) const noexcept -> iterator
    {
        return std::ranges::find(*this, ch);
    }

    constexpr auto find(string_view sv) const noexcept -> string_view
    {
        auto rg = std::ranges::search(as_bytes(), sv.as_bytes());

        auto [first, last] = rg;
        return {iterator{first.base()}, iterator{last.base()}};
    }

    constexpr auto rfind(value_type ch) const noexcept -> iterator
    {
        return std::prev(std::ranges::find(*this | std::views::reverse, ch).base());
    }

    constexpr auto rfind(string_view sv) const noexcept -> string_view
    {
        using std::views::reverse;
        auto rg = std::ranges::search(as_bytes() | reverse, sv.as_bytes() | reverse);

        auto [first, last] = rg | reverse;
        return {iterator{first.base()}, iterator{last.base()}};
    }


    constexpr auto to_std_string_view() const noexcept -> std::string_view
    {
        return std::string_view{reinterpret_cast<const char*>(first_.base()),
                                reinterpret_cast<const char*>(last_.base())};
    }

    friend constexpr auto operator==(string_view lhs, string_view rhs) noexcept -> bool
    {
        return std::ranges::equal(lhs, rhs);
    }

    friend constexpr auto operator<=>(string_view lhs, string_view rhs) noexcept -> std::strong_ordering
    {
        return std::lexicographical_compare_three_way(lhs.first_, lhs.last_, rhs.first_, rhs.last_);
    }
};

}
