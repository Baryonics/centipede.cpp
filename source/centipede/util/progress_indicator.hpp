#include "centipede/util/error_types.hpp"
#include <cassert>
#include <concepts>
#include <cstddef>
#include <functional>
#include <indicators/color.hpp>
#include <indicators/font_style.hpp>
#include <indicators/progress_bar.hpp>
#include <indicators/setting.hpp>
#include <memory>
#include <ranges>
#include <type_traits>
#include <utility>

namespace centipede
{
    // TODO:
    //  Error Handling!
    //  Ehm, testing?

    template <typename F>
    concept is_increment_function = std::invocable<F> && std::same_as<std::invoke_result_t<F>, std::size_t>;

    template <typename R>
    concept is_range = std::ranges::range<R>;

    class ProgressIndicator
    {
      private:
        template <is_range RangeT, is_increment_function IncrementFunctionT>
        struct ProgressView
        {
            using BaseView = std::views::all_t<RangeT>;

            ProgressView(BaseView&& view,
                         std::size_t total_size,
                         IncrementFunctionT&& inc_func,
                         ProgressIndicator* indicator)
                : base_view(std::move(view))
                , total_size_n(total_size)
                , increment_function(std::move(inc_func))
                , progress_indicator(indicator)
            {
            }

            auto begin() {}

            auto end() {}

            struct Sentinel
            {
            };

            struct Iterator
            {

                auto operator++() -> Iterator& {}

                auto operator*() {}

                bool operator==(Sentinel) {}

                bool operator!=(Sentinel sentinel) {}

                bool operator==(Sentinel) const {}

                bool operator!=(Sentinel sentinel) const {}
            };

            std::size_t total_size_n{};
            BaseView base_view{};
            IncrementFunctionT increment_function{};
            ProgressIndicator* progress_indicator = nullptr;
        };

        struct ProgressAdaptor : std::ranges::range_adaptor_closure<ProgressAdaptor>
        {
            ProgressAdaptor(ProgressIndicator* indicator)
                : progress_indicator(indicator)
            {
            }

            auto operator()(is_range auto&& range)
            {
                return ProgressView{ std::views::all(std::forward(range)),
                                     std::ranges::size(range),
                                     []() { return 1UZ; },
                                     progress_indicator };
            }

            auto operator()(is_range auto&& range, std::size_t total_size, is_increment_function auto&& inc_func)
            {
                return ProgressView{ std::views::all(std::forward(range), total_size, std::move(inc_func)) };
            }

            auto operator()(is_range auto&& range, std::size_t total_size)
            {
                return ProgressView{ std::views::all(std::forward(range), total_size, []() { return 1UZ; }) };
            }

            auto operator()(std::size_t total_size)
            {
                return ProgressClosure{ this, total_size, []() { return 1UZ; } };
            }

            auto operator()(std::size_t total_size, is_increment_function auto&& inc_func)
            {
                return ProgressClosure{ this, total_size, std::move(inc_func) };
            }

            template <is_increment_function IncrementFunctionT>
            struct ProgressClosure : std::ranges::range_adaptor_closure<ProgressClosure<IncrementFunctionT>>
            {
                ProgressClosure(ProgressAdaptor* adaptor, std::size_t total_size, IncrementFunctionT&& inc_func)
                    : progress_adaptor(adaptor)
                    , total_size_n(total_size)
                    , increment_function(std::move(inc_func))
                {
                }

                auto operator()(is_range auto&& range)
                {
                    return ProgressView{
                        std::views::all(std::forward(range)),
                        total_size_n,
                        std::move(increment_function),
                    };
                }

                ProgressAdaptor* progress_adaptor = nullptr;
                std::size_t total_size_n{};
                IncrementFunctionT increment_function{};
            };

            ProgressIndicator* progress_indicator = nullptr;
        };

        indicators::ProgressBar bar_{};
        ErrorCode status_{};

      public:
        ProgressAdaptor adaptor{ this };
    };

    /********************* OLD *************************/

    // using ProgressFontStyle = indicators::FontStyle;
    // using ProgressColor = indicators::Color;
    //
    // class ProgressAdaptor : public std::ranges::range_adaptor_closure<ProgressAdaptor>
    // {
    //   public:
    //     using IncrementFunT = std::function<std::size_t()>;
    //
    //     ProgressAdaptor() = default;
    //
    //     template <typename... Options>
    //     explicit ProgressAdaptor(Options&&... options)
    //         : bar_ptr_{ std::make_shared<indicators::ProgressBar>(std::forward<Options>(options)...) }
    //     {
    //     }
    //
    //     template <typename RangeT>
    //         requires std::ranges::range<RangeT>
    //     using BaseView = std::views::all_t<RangeT>;
    //
    //     struct ProgressClosure : std::ranges::range_adaptor_closure<ProgressClosure>
    //     {
    //         std::size_t total_size_n;
    //         IncrementFunT increment_fun;
    //
    //         std::shared_ptr<indicators::ProgressBar> bar_ptr;
    //         std::shared_ptr<ErrorCode> status_ptr;
    //
    //         template <typename RangeT>
    //             requires std::ranges::range<RangeT>
    //         auto operator()(RangeT&& range)
    //         {
    //             using ViewT = std::views::all_t<RangeT>;
    //
    //             return ProgressView<ViewT>{
    //                 std::views::all(std::forward<RangeT>(range)), total_size_n, increment_fun, bar_ptr,
    //                 status_ptr
    //             };
    //         }
    //     };
    //
    //     template <typename RangeT>
    //         requires std::ranges::range<RangeT>
    //     auto operator()(RangeT&& range, std::size_t total_size_n, IncrementFunT increment_fun)
    //     {
    //         return ProgressView<BaseView<RangeT>>{ std::views::all(std::forward<RangeT>(range)),
    //                                                total_size_n,
    //                                                std::move(increment_fun),
    //                                                bar_ptr_,
    //                                                status_ptr_ };
    //     }
    //
    //     template <typename RangeT>
    //         requires std::ranges::range<RangeT>
    //     auto operator()(RangeT&& range, std::size_t total_size_n)
    //     {
    //         return ProgressView<BaseView<RangeT>>{ std::views::all(std::forward<RangeT>(range)),
    //                                                total_size_n,
    //                                                []() -> std::size_t { return 1UZ; },
    //                                                bar_ptr_,
    //                                                status_ptr_ };
    //     }
    //
    //     template <typename RangeT>
    //         requires std::ranges::sized_range<RangeT>
    //     auto operator()(RangeT&& range)
    //     {
    //         return ProgressView<BaseView<RangeT>>{ std::views::all(std::forward<RangeT>(range)),
    //                                                std::ranges::size(range),
    //                                                []() -> std::size_t { return 1UZ; },
    //                                                bar_ptr_,
    //                                                status_ptr_ };
    //     }
    //
    //     auto operator()(std::size_t total_size_n)
    //     {
    //         return ProgressClosure{ {}, total_size_n, []() -> std::size_t { return 1UZ; }, bar_ptr_, status_ptr_
    //         };
    //     }
    //
    //     auto operator()(std::size_t total_size_n, IncrementFunT increment_fun)
    //     {
    //         return ProgressClosure{ {}, total_size_n, std::move(increment_fun), bar_ptr_, status_ptr_ };
    //     }
    //
    //     [[nodiscard]] auto get_status() const -> ErrorCode { return *status_ptr_; }
    //
    //     template <typename RangeT>
    //         requires std::ranges::range<RangeT>
    //     struct ProgressView
    //     {
    //         using BaseView = std::views::all_t<RangeT>;
    //         using IteratorType = std::ranges::iterator_t<RangeT>;
    //         using SentinelType = std::ranges::sentinel_t<RangeT>;
    //
    //         ProgressView(BaseView base_view,
    //                      std::size_t total_size_n,
    //                      IncrementFunT increment_fun,
    //                      std::shared_ptr<indicators::ProgressBar> bar_ptr,
    //                      std::shared_ptr<ErrorCode> status_ptr)
    //             : base_view_(std::move(base_view))
    //             , total_size_n_(total_size_n)
    //             , increment_fun_(std::move(increment_fun))
    //             , bar_ptr_(std::move(bar_ptr))
    //             , status_ptr_(std::move(status_ptr))
    //         {
    //             assert(bar_ptr_);
    //             assert(status_ptr_);
    //             assert(increment_fun_);
    //         }
    //
    //         auto get_status() -> ErrorCode { return *status_ptr_; }
    //
    //         auto begin()
    //         {
    //             if (total_size_n_ == 0UZ)
    //             {
    //                 *status_ptr_ = ErrorCode::progress_zero_size;
    //
    //                 bar_ptr_->mark_as_completed();
    //             }
    //
    //             return Iterator{ this, std::ranges::begin(base_view_), std::ranges::end(base_view_) };
    //         }
    //
    //         auto end() { return Sentinel{}; }
    //
    //         struct Sentinel
    //         {
    //         };
    //
    //         class Iterator
    //         {
    //           public:
    //             Iterator(ProgressView* progress_view, IteratorType current_it, SentinelType end_it)
    //                 : progress_view_(progress_view)
    //                 , current_it_(current_it)
    //                 , end_it_(end_it)
    //             {
    //                 assert(progress_view_);
    //             }
    //
    //             auto operator++() -> Iterator&
    //             {
    //                 assert(current_it_ != end_it_);
    //                 add_progress();
    //                 ++current_it_;
    //                 return *this;
    //             }
    //
    //             auto operator*()
    //             {
    //                 assert(current_it_ != end_it_);
    //                 return *current_it_;
    //             }
    //
    //             bool operator==(Sentinel) { return current_it_ == end_it_; }
    //
    //             bool operator!=(Sentinel sentinel) { return !(*this == sentinel); }
    //
    //             bool operator==(Sentinel) const { return current_it_ == end_it_; }
    //
    //             bool operator!=(Sentinel sentinel) const { return !(*this == sentinel); }
    //
    //             void add_progress()
    //             {
    //                 assert(progress_view_);
    //                 assert(progress_view_->bar_ptr_);
    //                 assert(progress_view_->status_ptr_);
    //                 assert(progress_view_->increment_fun_);
    //                 assert(count_n_ <= progress_view_->total_size_n_);
    //                 const auto increment = progress_view_->increment_fun_();
    //
    //                 if (increment == 0UZ)
    //                 {
    //                     *progress_view_->status_ptr_ = ErrorCode::progress_inc_returns_zero;
    //                     return;
    //                 }
    //
    //                 const auto remaining = progress_view_->total_size_n_ - count_n_;
    //
    //                 if (increment > remaining)
    //                 {
    //                     count_n_ = progress_view_->total_size_n_;
    //
    //                     *progress_view_->status_ptr_ = ErrorCode::progress_inc_exceeds_size;
    //                 }
    //                 else
    //                 {
    //                     count_n_ += increment;
    //                 }
    //
    //                 const auto percent = 100UZ * count_n_ / progress_view_->total_size_n_;
    //
    //                 progress_view_->bar_ptr_->set_progress(percent);
    //
    //                 if (count_n_ == progress_view_->total_size_n_)
    //                 {
    //                     *(progress_view_->status_ptr_) = ErrorCode::success;
    //                 }
    //             }
    //
    //           private:
    //             ProgressView* progress_view_;
    //             IteratorType current_it_;
    //             SentinelType end_it_;
    //             std::size_t count_n_{};
    //         };
    //
    //       private:
    //         BaseView base_view_;
    //         std::size_t total_size_n_;
    //         IncrementFunT increment_fun_;
    //         std::shared_ptr<indicators::ProgressBar> bar_ptr_ = nullptr;
    //         std::shared_ptr<ErrorCode> status_ptr_ = nullptr;
    //     };
    //
    //   private:
    //     std::shared_ptr<indicators::ProgressBar> bar_ptr_{ std::make_shared<indicators::ProgressBar>() };
    //     std::shared_ptr<ErrorCode> status_ptr_ = std::make_shared<ErrorCode>(ErrorCode::incomplete);
    // };
} // namespace centipede
