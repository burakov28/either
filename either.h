#include <iostream>
#include <memory>
#include <cassert>
#include <iostream>

struct emplace_left_t {};
struct emplace_right_t {};

emplace_left_t emplace_left;
emplace_right_t emplace_right;


template <typename Left, typename Right>
struct either {
  private:
    static const char LEFT = 0;
    static const char RIGHT = 1;
    static const char LEFT_PTR = 2;
    static const char RIGHT_PTR = 3;

    
    char storage[std::max(sizeof(Left), std::max(sizeof(void*), sizeof(Right)))] alignas(std::max(alignof(Left), std::max(alignof(void*), alignof(Right))));
    char state;

    template <typename ME, typename HE, typename... Args>
    void emplace_no_ptr(Args&&... args) {
        std::unique_ptr<ME> tmp = std::make_unique<ME>(*((ME*) storage));
        try {
            ((ME*) storage) -> ~ME();
            new (storage) HE(std::forward<Args>(args)...);
            
            if (std::is_same<HE, Left>::value) state = LEFT;
            else state = RIGHT;
            tmp.reset();
        }
        catch (...) {
            new (storage) ME* (tmp.release());            
            throw;
        }
        tmp.reset();
        
    }

    template <typename ME, typename HE, typename... Args>
    void emplace_ptr(Args&&... args) {
        ME* tmp = *((ME**) storage);
        try {
            new (storage) HE(std::forward<Args>(args)...);
            if (std::is_same<HE, Left>::value) state = LEFT;
            else state = RIGHT;
            delete tmp;
        }
        catch (...) {
            new (storage) ME*(tmp);
            throw;
        }
    }

    template <typename HE, typename... Args>
    void disp_emplace (Args&&... args) {
        if (state == LEFT) {
            emplace_no_ptr<Left, HE, Args...> (std::forward<Args>(args)...);
            return;
        }                                                                                              
        if (state == LEFT_PTR) {
            emplace_ptr<Left, HE, Args...> (std::forward<Args>(args)...);
            return;  
        }
        if (state == RIGHT) {
            emplace_no_ptr<Right, HE, Args...> (std::forward<Args>(args)...);
            return;
        }
        if (state == RIGHT_PTR) {
            emplace_ptr<Right, HE, Args...> (std::forward<Args>(args)...);
            return;
        } 
    }

    
    void deleter_() {
        switch (state) {
            case LEFT:
                ((Left*) storage) -> ~Left();
                break;
            case LEFT_PTR:
                delete *((Left**) storage);
                break;
            case RIGHT:
                ((Right*) storage) -> ~Right();
                break;
            case RIGHT_PTR:
                delete *((Right**) storage);
                break;        
        }        
    }
    
    void build_ref(Left* ref) {
        deleter_();
        state = LEFT_PTR;      
        new (storage) Left*(ref);
    }


    void build_ref(Right* ref) {
        deleter_();        
        state = RIGHT_PTR;
        new (storage) Right*(ref);
    }


  public:
    either(Left left) {
        new (storage) Left(std::move(left));
        state = LEFT;
    }

    either(Right right) {
        new (storage) Right(std::move(right));
        state = RIGHT;
    }

    either(const either & other) {
        if (other.is_left()) {
            state = LEFT;
            new (storage) Left(other.get_left());
        }
        else {
            state = RIGHT;
            new (storage) Right(other.get_right());  
        }
    }

    either(either && other) {
        if (other.is_left()) {
            state = LEFT;
            new (storage) Left(std::move(other.get_left()));
        }
        else {
            state = RIGHT;
            new (storage) Right(std::move(other.get_right()));  
        }
    }

    template <typename... Args>
    either(emplace_left_t const &, Args&&... args) {
        state = LEFT;
        new (storage) Left(std::forward<Args>(args)...); 
    }

    template <typename... Args>
    either(emplace_right_t const &, Args&&... args) {
        state = RIGHT;
        new (storage) Right(std::forward<Args>(args)...); 
    }

    either & operator=(either other) {
        if (other.is_left()) {
            emplace(emplace_left_t(), other.get_left());
        }
        else {
            emplace(emplace_right_t(), other.get_right());
        }
        return *this;
    }

    ~either() {
        switch (state) {
            case LEFT:
                ((Left*) storage) -> ~Left();
                break;
            case LEFT_PTR:
                delete *((Left**) storage);
                break;
            case RIGHT:
                ((Right*) storage) -> ~Right();
                break;
            case RIGHT_PTR:
                delete *((Right**) storage);
                break;
        }
    }

    bool is_left() const {
        return (state == LEFT || state == LEFT_PTR);
    }

    bool is_right() const {
        return (state == RIGHT || state == RIGHT_PTR);
    }

    Left & get_left() {
        if (state == LEFT) {
            return *((Left*) storage);
        }
        if (state == LEFT_PTR) {
            return **((Left**) storage);
        }
        throw std::string("wrong arg");
        //assert(false);
    }

    Left const & get_left() const {
        if (state == LEFT) {
            return *((Left*) storage);
        }
        if (state == LEFT_PTR) {
            return **((Left**) storage);
        }
        throw std::string("wrong arg");
        //assert(false);
    }

    Right & get_right() {
        if (state == RIGHT) {
            return *((Right*) storage);
        }
        if (state == RIGHT_PTR) {
            return **((Right**) storage);
        }
        throw std::string("wrong arg");
    }

    Right const & get_right() const {
        if (state == RIGHT) {
            return *((Right*) storage);
        }
        if (state == RIGHT_PTR) {
            return **((Right**) storage);
        }
        throw std::string("wrong arg");
    }    

    template <typename... Args>
    void emplace(emplace_left_t, Args&&... args) {
        disp_emplace<Left, Args...> (std::forward<Args>(args)...); 
    }
    template <typename... Args>
    void emplace(emplace_right_t, Args&&... args) {
        disp_emplace<Right, Args...> (std::forward<Args>(args)...); 
    }

       
    template <typename _Left, typename _Right>
    friend void swap(either <_Left, _Right> & e1, either <_Left, _Right> & e2);
};

template <typename F, typename Left, typename Right>
decltype(auto) apply(F func, either <Left, Right>& e) {
    if (e.is_left()) return func(e.get_left());
    else return func(e.get_right());
}

template <typename F, typename Left, typename Right>
decltype(auto) apply(F func, const either <Left, Right>& e) {
    if (e.is_left()) return func(e.get_left());
    else return func(e.get_right());
}


template <typename Left, typename Right>
void swap(either <Left, Right> & e1, either <Left, Right> & e2) {
    
    if (e1.is_left()) {
        Left * tmp1 = new Left(e1.get_left());    
        if (e2.is_left()) {
            Left * tmp2 = new Left(e2.get_left());
            try {
                e1.emplace(emplace_left_t(), *tmp2);
                e2.emplace(emplace_left_t(), *tmp1);
                delete tmp1;
                delete tmp2;
            }
            catch (...) {
                e1.build_ref(tmp1);
                e2.build_ref(tmp2);
                throw;
            }
        }
        else {
            Right * tmp2 = new Right(e2.get_right());
        
            try {
                e1.emplace(emplace_right_t(), *tmp2);
                e2.emplace(emplace_left_t(), *tmp1);
                delete tmp1;
                delete tmp2;
            }
            catch (...) {
                e1.build_ref(tmp1);
                e2.build_ref(tmp2);
                throw;
            }
        }
    }
    else {
        Right * tmp1 = new Right(e1.get_right());
        if (e2.is_left()) {
            Left * tmp2 = new Left(e2.get_left());
            try {
                e1.emplace(emplace_left_t(), *tmp2);
                e2.emplace(emplace_right_t(), *tmp1);
                delete tmp1;
                delete tmp2;
            }
            catch (...) {
                e1.build_ref(tmp1);
                e2.build_ref(tmp2);
                throw;
            }
        }
        else {
            Right * tmp2 = new Right(e2.get_right());
        
            try {
                e1.emplace(emplace_right_t(), *tmp2);
                e2.emplace(emplace_right_t(), *tmp1);
                delete tmp1;
                delete tmp2;
            }
            catch (...) {
                e1.build_ref(tmp1);
                e2.build_ref(tmp2);
                throw;
            }
        }   
    }   
}

