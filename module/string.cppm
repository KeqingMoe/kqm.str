export module kqm.str.string;

import std;
import kqm.str.utf;
import kqm.str.view;

struct const_propagator
{
    template <typename T, typename Self>
    constexpr auto operator()(this Self&, T* ptr) noexcept
    {
        if constexpr (std::is_const_v<Self>) {
            return static_cast<const T*>(ptr);
        } else return ptr;
    }
};

struct dyn_t
{
    std::size_t siz;
    std::size_t cap;
};

struct sso_t
{
    std::uint8_t siz;
    std::byte dat[sizeof(dyn_t) - 1uz];
};

constexpr auto sso_size = sizeof(sso_t::dat);

using byte_span = std::span<std::byte>;
using byte_view = std::span<const std::byte>;

template <class R>
concept Utf32Range =
    std::ranges::input_range<R> && std::same_as<std::remove_cvref_t<std::ranges::range_reference_t<R>>, char32_t>;

template <class R>
concept Utf8ByteRange =
    std::ranges::input_range<R> && kqm::utf::Utf8CharOrByte<std::remove_cvref_t<std::ranges::range_reference_t<R>>>;

export namespace kqm
{

template <typename Allocator = std::allocator<std::byte>>
struct basic_string
{
public:
    using value_type             = char32_t;
    using allocator_type         = Allocator;
    using size_type              = std::allocator_traits<Allocator>::size_type;
    using difference_type        = std::allocator_traits<Allocator>::difference_type;
    using const_iterator         = utf::utf8_to_utf32_iterator<const std::byte*>;
    using iterator               = const_iterator;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    using reverse_iterator       = const_reverse_iterator;

private:
    std::byte* mem_;
    union
    {
        dyn_t dyn_;
        sso_t sso_;
    };
    [[no_unique_address]] std::allocator<std::byte> alloc_;
    [[no_unique_address]] const_propagator cp_;


    constexpr auto replace_impl(byte_span dest, byte_view data)
    {
        auto [new_siz, allocs] = [&] {
            auto old_siz = size_bytes();
            auto rem_siz = dest.size();
            auto rep_siz = data.size();
            auto tmp_siz = old_siz - rem_siz;
            if (max_size_bytes() - tmp_siz < rep_siz) {
                throw std::length_error{"size() + delta-size > max_size()"};
            }
            auto new_siz = tmp_siz + rep_siz;
            auto new_cap = std::max(old_siz * 2, new_siz);
            return std::tuple{new_siz, new_cap};
        }();
        auto dst = dest.data();

        auto first = this->data();
        auto mid   = dest.data();
        auto last  = dest.end().base();
        auto tail  = first + size_bytes();

        auto fsize = mid - first;
        auto msize = dest.size();
        auto lsize = tail - last;

        if (auto cap = capacity(); new_siz > cap) {
            auto [new_mem, new_cap] = alloc_.allocate_at_least(allocs);
            std::memcpy(new_mem, first, fsize);
            std::memcpy(new_mem + fsize, data.data(), data.size());
            std::memcpy(new_mem + fsize + data.size(), last, lsize);
            if (mem_) {
                alloc_.deallocate(mem_, dyn_.cap);
            }
            mem_ = new_mem;
            dyn_ = dyn_t{new_siz, new_cap};
        } else {
            std::memmove(mid + data.size(), last, lsize);
            if (!data.empty()) {
                std::memcpy(dst, data.data(), data.size());
            }
            if (mem_) {
                dyn_.siz = new_siz;
            } else {
                sso_.siz = new_siz;
            }
        }
    }

    constexpr auto replace_impl(byte_view dest, byte_view data)
    {
        replace_impl(byte_span{const_cast<std::byte*>(dest.data()), dest.size()}, data);
    }

    constexpr auto move_impl(basic_string& other) noexcept
    {
        if (other.mem_) {
            mem_ = other.mem_;
            dyn_ = other.dyn_;

            other.mem_ = nullptr;
            other.sso_ = {};
        } else {
            sso_.siz = other.sso_.siz;
            memcpy(sso_.dat, other.sso_.dat, sso_.siz);
        }
    }

public:
    constexpr basic_string() noexcept(std::is_nothrow_default_constructible_v<Allocator>) : mem_{}, sso_{}, alloc_{} {}

    constexpr basic_string(allocator_type alloc) noexcept : mem_{}, sso_{}, alloc_{std::move(alloc)} {}

    template <std::input_iterator Iter, std::sentinel_for<Iter> S>
        requires std::convertible_to<std::iter_value_t<Iter>, char32_t>
    constexpr basic_string(Iter first, S last, allocator_type alloc = allocator_type{}) : basic_string{std::move(alloc)}
    {
        append(std::ranges::subrange{first, last});
    }

    constexpr basic_string(string_view sv, allocator_type alloc = allocator_type{}) : basic_string{std::move(alloc)}
    {
        append(sv);
    }

    template <std::size_t N>
    constexpr basic_string(const char (&str)[N], allocator_type alloc = allocator_type{})
        : basic_string{std::move(alloc)}
    {
        append(string_view{
            std::string_view{str, N}
        });
    }

    constexpr basic_string(std::nullptr_t) = delete;

    constexpr basic_string(const basic_string& other) : basic_string{other.alloc_}
    {
        assign(other.all());
    }

    constexpr basic_string(basic_string&& other) noexcept : alloc_{std::move(other.alloc_)}
    {
        move_impl(other);
    }

    constexpr ~basic_string()
    {
        if (mem_) {
            alloc_.deallocate(mem_, dyn_.cap);
        }
    }

    constexpr auto operator=(const basic_string& other) & -> basic_string&
    {
        if (this != &other) {
            assign(other);
        }
        return *this;
    }

    constexpr auto operator=(basic_string&& other) & noexcept(
        std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value
        || std::allocator_traits<Allocator>::is_always_equal::value) -> basic_string&
    {
        std::swap(alloc_, other.alloc_);
        if (mem_) {
            other.alloc_.deallocate(mem_, dyn_.siz);
        }
        move_impl(other);
        return *this;
    }

    constexpr auto operator=(string_view other) & -> basic_string&
    {
        if (this != &other) {
            assign(other);
        }
        return *this;
    }

    auto get_allocator() const -> allocator_type
    {
        return alloc_;
    }

    constexpr auto front() const noexcept -> value_type
    {
        return *begin();
    }

    constexpr auto back() const noexcept -> value_type
    {
        return *(std::prev(end()));
    }


    constexpr auto begin(this auto& self) noexcept
    {
        return utf::utf8_to_utf32_iterator{self.cp_(self.data())};
    }

    constexpr auto begin() const&& noexcept -> std::ranges::dangling
    {
        return {};
    }

    constexpr auto end(this auto& self) noexcept
    {
        return utf::utf8_to_utf32_iterator{self.cp_(self.data() + self.size_bytes())};
    }

    constexpr auto end() const&& noexcept -> std::ranges::dangling
    {
        return {};
    }

    constexpr auto cbegin() const& noexcept -> iterator
    {
        return begin();
    }

    constexpr auto cbegin() const&& noexcept -> std::ranges::dangling
    {
        return {};
    }

    constexpr auto cend() const& noexcept -> iterator
    {
        return end();
    }

    constexpr auto cend() const&& noexcept -> std::ranges::dangling
    {
        return {};
    }

    constexpr auto rbegin(this auto& self) noexcept
    {
        return std::reverse_iterator{self.begin()};
    }

    constexpr auto rbegin() const&& noexcept -> std::ranges::dangling
    {
        return {};
    }

    constexpr auto rend(this auto& self) noexcept
    {
        return std::reverse_iterator{self.end()};
    }

    constexpr auto rend() const&& noexcept -> std::ranges::dangling
    {
        return {};
    }

    constexpr auto crbegin() const& noexcept -> const_reverse_iterator
    {
        return rbegin();
    }

    constexpr auto crbegin() const&& noexcept -> std::ranges::dangling
    {
        return {};
    }

    constexpr auto crend() const& noexcept -> const_reverse_iterator
    {
        return rend();
    }

    constexpr auto crend() const&& noexcept -> std::ranges::dangling
    {
        return {};
    }

    constexpr auto clear() & noexcept -> void
    {
        replace_impl(as_const_bytes(), {});
    }

    constexpr auto insert(iterator pos, value_type ch) & -> basic_string&
    {
        auto buf  = std::array<std::byte, 4uz>{};
        auto last = utf::utf32_to_utf8(ch, buf.data());
        replace_impl({pos.base(), 0uz}, {buf.data(), last});
    }

    constexpr auto erase(iterator first, iterator last) & noexcept -> iterator
    {
        replace_impl({first.base(), last.base()}, {});
        return iterator{const_cast<std::byte*>(first.base())};
    }

    constexpr auto erase(iterator pos) & noexcept -> iterator
    {
        return erase(pos, end());
    }

    constexpr auto push_back(value_type ch) & -> void
    {
        insert(end(), ch);
    }

    constexpr auto pop_back() & noexcept -> void
    {
        auto last  = end();
        auto first = std::prev(last);
        replace_impl({first.base(), last.base()}, {});
    }

    constexpr auto append(Utf32Range auto&& rg) & -> auto&&
    {
        for (auto ch : rg) {
            push_back(ch);
        }
        return *this;
    }

    constexpr auto append(value_type ch) & -> auto&&
    {
        append(std::array{ch});
        return *this;
    }

    constexpr auto append(Utf8ByteRange auto&& rg) & -> auto&&
    {
        if (!std::ranges::empty(rg)) {
            auto pos = byte_view{end().base(), 0uz};
            if constexpr (std::ranges::contiguous_range<decltype(rg)>) {
                replace_impl(pos, std::as_bytes(std::span{std::ranges::data(rg), std::ranges::size(rg)}));
            } else {
                for (auto byte : rg) {
                    replace_impl(pos, {&byte, 1uz});
                }
            }
        }
        return *this;
    }

    constexpr auto append(string_view str) & -> auto&&
    {
        append(str.as_bytes());
        return *this;
    }

    constexpr auto operator+=(auto&& rhs) & -> auto&&
    {
        append(std::forward<decltype(rhs)>(rhs));
        return *this;
    }

    constexpr auto replace(iterator first, iterator last, string_view str) & -> auto&&
    {
        replace_impl({first.base(), last.base()}, str.as_bytes());
        return *this;
    }

    constexpr auto replace(iterator first, iterator last, Utf32Range auto&& rg) & -> auto&&
    {
        auto buf = basic_string{alloc_};
        buf.append(rg);
        replace(first, last, buf);
        return *this;
    }

    constexpr auto replace(iterator first, iterator last, Utf8ByteRange auto&& rg) & -> auto&&
    {
        if (!std::ranges::empty(rg)) {
            if constexpr (std::ranges::contiguous_range<decltype(rg)>) {
                replace_impl({first.base(), last.base()},
                             std::as_bytes(std::span{std::ranges::data(rg), std::ranges::size(rg)}));
            } else {
                auto buf = basic_string{alloc_};
                buf.append(rg);
                replace(first, last, buf);
            }
        }
        return *this;
    }

    constexpr auto replace(iterator first, iterator last, value_type ch) & -> auto&&
    {
        auto buf      = std::array<std::byte, 4uz>{};
        auto buf_last = utf::utf32_to_utf8(ch, buf.data());
        replace(first, last, byte_view{buf.data(), buf_last});
        return *this;
    }

    constexpr auto assign(string_view other) & -> auto&&
    {
        clear();
        append(other.as_bytes());
        return *this;
    }

    constexpr auto assign(Utf32Range auto&& rg) & -> auto&&
    {
        clear();
        append(std::forward<decltype(rg)>(rg));
        return *this;
    }

    constexpr auto assign(Utf8ByteRange auto&& rg) & -> auto&&
    {
        clear();
        append(std::forward<decltype(rg)>(rg));
        return *this;
    }

    constexpr auto assign(value_type ch) & -> auto&&
    {
        clear();
        append(ch);
        return *this;
    }


    constexpr auto empty() const noexcept -> bool
    {
        return !mem_ && !sso_.siz;
    }

    constexpr auto size_bytes() const noexcept -> std::size_t
    {
        if (mem_) {
            return dyn_.siz;
        } else {
            return sso_.siz;
        }
    }

    static constexpr auto max_size_bytes() noexcept -> std::size_t
    {
        return std::numeric_limits<std::ptrdiff_t>::max();
    }

    constexpr auto capacity() const noexcept -> std::size_t
    {
        if (mem_) {
            return dyn_.cap;
        } else {
            return sso_size;
        }
    }

    constexpr auto reserve(std::size_t new_cap) & -> void
    {
        if (new_cap > max_size_bytes()) {
            throw std::length_error{"new_cap > max_size_bytes()"};
        }
        auto [cap, mem, siz] = [this] {
            if (mem_) {
                return std::tuple{dyn_.cap, mem_, dyn_.siz};
            } else {
                return std::tuple{sso_size, sso_.dat, sso_.siz};
            }
        }();
        if (new_cap < cap) return;
        auto [new_mem, alloc_siz] = alloc_.allocate_at_least(new_cap);
        std::memcpy(new_mem, mem, siz);
        if (mem_) {
            alloc_.deallocate(mem_, dyn_.cap);
            dyn_.cap = alloc_siz;
        } else {
            sso_.siz = alloc_siz;
        }
        mem_ = new_mem;
    }

    constexpr auto shrink_to_fit() & -> void {}

    constexpr auto
    swap(basic_string& other) & noexcept(std::allocator_traits<Allocator>::propagate_on_container_swap::value
                                         || std::allocator_traits<Allocator>::is_always_equal::value) -> void
    {
        if (this == &other) {
            auto tmp = std::move(other);
            other    = std::move(*this);
            *this    = std::move(tmp);
        }
    }

    constexpr auto data(this auto& self) noexcept
    {
        if (self.mem_) {
            return self.cp_(self.mem_);
        } else {
            return self.sso_.dat;
        }
    }

    constexpr auto data() const&& noexcept -> std::ranges::dangling
    {
        return {};
    }

    constexpr auto as_bytes(this auto& self) noexcept
    {
        return std::span{self.data(), self.bytes()};
    }

    constexpr auto as_bytes() const&& noexcept -> std::ranges::dangling
    {
        return {};
    }

    constexpr auto as_const_bytes() const& noexcept -> std::span<const std::byte>
    {
        return std::span{data(), size_bytes()};
    }

    constexpr auto as_const_bytes() const&& noexcept -> std::ranges::dangling
    {
        return {};
    }

    constexpr auto all() const& noexcept -> string_view
    {
        return {begin(), end()};
    }

    constexpr auto all() const&& noexcept -> std::ranges::dangling
    {
        return {};
    }

    constexpr operator string_view() const& noexcept
    {
        return all();
    }

    constexpr operator string_view() const&& noexcept = delete;

    constexpr auto start_with(value_type ch) const noexcept -> bool
    {
        return all().starts_with(ch);
    }

    constexpr auto start_with(string_view sv) const noexcept -> bool
    {
        return all().starts_with(sv);
    }

    constexpr auto ends_with(value_type ch) const noexcept -> bool
    {
        return all().ends_with(ch);
    }

    constexpr auto ends_with(string_view sv) const noexcept -> bool
    {
        return all().ends_with(sv);
    }

    constexpr auto contains(value_type ch) const noexcept -> bool
    {
        return all().contains(ch);
    }

    constexpr auto contains(string_view sv) const noexcept -> bool
    {
        return all().contains(sv);
    }

    constexpr auto find(value_type ch) const& noexcept -> iterator
    {
        return all().find(ch);
    }

    constexpr auto find(value_type ch) const&& noexcept -> std::ranges::dangling
    {
        return {};
    }

    constexpr auto find(string_view sv) const& noexcept -> string_view
    {
        return all().find(sv);
    }

    constexpr auto find(string_view sv) const&& noexcept -> std::ranges::dangling
    {
        return {};
    }

    constexpr auto rfind(value_type ch) const& noexcept -> iterator
    {
        return all().rfind(ch);
    }

    constexpr auto rfind(value_type ch) const&& noexcept -> std::ranges::dangling
    {
        return {};
    }

    constexpr auto rfind(string_view sv) const& noexcept -> string_view
    {
        return all().rfind(sv);
    }

    constexpr auto rfind(string_view sv) const&& noexcept -> std::ranges::dangling
    {
        return {};
    }


    constexpr auto to_std_string() const
    {
        using ret_t = std::basic_string<char, std::char_traits<char>, allocator_type>;
        return ret_t{all(), alloc_};
    }

    friend constexpr auto operator==(const basic_string& lhs, const basic_string& rhs) noexcept -> bool
    {
        return lhs.all() == rhs.all();
    }

    friend constexpr auto operator<=>(const basic_string& lhs, const basic_string& rhs) noexcept -> std::strong_ordering
    {
        return lhs.all() <=> rhs.all();
    }

    friend constexpr auto operator+(const basic_string& lhs, const basic_string& rhs) noexcept -> basic_string
    {
        auto ret = lhs;
        ret.append(rhs);
        return ret;
    }

    friend constexpr auto operator+(const basic_string& lhs, string_view rhs) noexcept -> basic_string
    {
        auto ret = lhs;
        ret.append(rhs);
        return ret;
    }

    friend constexpr auto operator+(const basic_string& lhs, value_type rhs) noexcept -> basic_string
    {
        auto ret = lhs;
        ret.append(rhs);
        return ret;
    }

    friend constexpr auto operator+(string_view lhs, const basic_string& rhs) noexcept -> basic_string
    {
        auto ret = basic_string{lhs};
        ret.append(rhs);
        return ret;
    }

    friend constexpr auto operator+(value_type lhs, const basic_string& rhs) noexcept -> basic_string
    {
        auto ret = basic_string{lhs};
        ret.append(rhs);
        return ret;
    }
};

template <typename Allocator>
constexpr auto swap(basic_string<Allocator>& lhs, basic_string<Allocator>& rhs) noexcept(noexcept(lhs.swap(rhs)))
{
    lhs.swap(rhs);
}

using string = basic_string<>;

namespace pmr
{

using string = basic_string<std::pmr::polymorphic_allocator<std::byte>>;

}

}

export template <typename Allocator>
struct std::formatter<kqm::basic_string<Allocator>, char> : std::formatter<kqm::string_view, char>
{
    auto format(const kqm::basic_string<Allocator>& s, auto&& ctx) const
    {
        return std::formatter<kqm::string_view, char>::format(s.all(), ctx);
    }
};
